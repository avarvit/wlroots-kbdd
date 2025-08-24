#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <wayland-client.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <dlfcn.h>
#include "kbdd.h"

// the file descriptor on which we receive SIGHUP (set up via signalfd(2))
static int sighup_fd = -1;

// labwc uses SIGHUP for reconfiguratin. We hijack SIGHUP (see below), and
// manage for labwc to receive a SIGUSR1 instead and process it exactly as
// it would have done for SIGHUP (also see below), while at the same time
// catching and processing ourselves SIGHUP. Below is our SIGHUP handler
// that sends SIGUSR1 and flags that a reconfiguration is pending

static struct kbdd_context *context = NULL;

static void sighup_handler(int signum) {
    kbdd_log("SIGHUP received\n");
    
    // if we have set up parent to receive SIGUSR1 on ex-sighup file descriptor, send SIGUSR1
    if (sighup_fd >= 0) {
        kbdd_log("Sending SIGUSR1 to myself\n");

        // flag we must re-check environment
        context->expect_envchange = true;

        // tell labwc to recheck configuration (after our hack, via SIGUSR1)
        kill(getpid(), SIGUSR1);
    }
}

// In order to avoid getenv()ing XKB_DEFAULT_LAYOUT each time, which
// may be wasteful, we try to intercept SIGHUP. This sounds easier than
// it actually is, because the wayland server core library uses signalfd(2)
// to set up signals and poll them asynchronously in event loops (and
// blocks the respective signal in the process's sigmask). In order to
// cope with this, we need to find the file descriptor that our process
// uses for SIGHUP signalfd(2), disable SIGHUP delivery to that fd (Linux
// cannot handle delivery of the same signal to multiple fd's) and unmask
// SIGHUP so we can catch it via the traditional signal(2) method; to
// allow the compositor to initialize upon SIGHUP, we exchange the found
// signal delieverd to the file descriptor with SIGUSR1, and send SIGUSR1
// to ourselves when we receive a SIGHUP (and yes, this works).

// while we could use PATH_MAX, this (4096) would be too wasteful in our
// case, since the paths we fiddle with under /proc (/proc/<pid>/fd/* and
// /proc/<pid>/fdinfo/*) should not be that long, so we use a smaller
// maximum path length
#define PROC_PATH_MAX 128

int initialize_sighup(struct kbdd_context *ctx) {

    if (sighup_fd != -1) {
        return sighup_fd;
    }

    context = ctx;

    // open directory /proc/<pid>/fd for reading
    pid_t pid = getpid();
    char proc_fd_dir_path[PROC_PATH_MAX];               // /proc/<pid>/fd
    char proc_fdinfo_dir_path[PROC_PATH_MAX];           // /proc/<pid>/fdinfo

    snprintf(proc_fd_dir_path, PROC_PATH_MAX, "/proc/%d/fd", pid);
    proc_fd_dir_path[PROC_PATH_MAX - 1] = 0;

    snprintf(proc_fdinfo_dir_path, PROC_PATH_MAX, "/proc/%d/fdinfo", pid);
    proc_fdinfo_dir_path[PROC_PATH_MAX - 1] = 0;

    char proc_fd_path[PROC_PATH_MAX];                   // /proc/<pid>/fd/<fd>
    char pointing_to[PROC_PATH_MAX];                    // symlink-pointed to path
    char fdinfo_path[PROC_PATH_MAX];                    // /proc/<pid>/fdinfo/<fd>

    // open /proc/<pid>/fd for listing

    DIR *proc_fd_dir = opendir(proc_fd_dir_path);
    if (proc_fd_dir == NULL) {
        perror(proc_fd_dir_path);
        sighup_fd = -2;         // failure
        return sighup_fd;
    }

    // loop over each file under /proc/<pid>/fd/ trying to find the right one
    // "the right one" (a) is a symlink (ignore other types), (b) instead of
    // pointing to a path, points to the string "anon_inode:[signalfd]"; at
    // this first stage, we need to examine further each such file, because
    // the actual signal is only specified within /proc/<pid>/fdinfo/<fd>
    // for the same <fd>

    struct dirent *entry;
    while ((entry = readdir(proc_fd_dir)) != NULL) {

        // skip non-symlinks
        if (!(entry->d_type & DT_LNK)) {
            continue;
        }

        // construct /proc/<<pid>/fd/<fd>, where <fd> is the name from readdir(2)

        snprintf(proc_fd_path, PROC_PATH_MAX, "%s", proc_fd_dir_path);
        strncat(proc_fd_path, "/", PROC_PATH_MAX - strlen(proc_fd_path) - 1);
        strncat(proc_fd_path, entry->d_name, PROC_PATH_MAX - strlen(proc_fd_path) - 1);
        proc_fd_path[PROC_PATH_MAX - 1] = 0;

        // check where the link points to

        ssize_t linksize = readlink(proc_fd_path, pointing_to, PROC_PATH_MAX);
        if (linksize < 0) {
            perror("readlink failed");
            continue;
        }
        pointing_to[linksize] = 0;


        // check if it is a signalfd anon inode

        if (strcmp (pointing_to, "anon_inode:[signalfd]") != 0) {
            continue;
        }

        kbdd_log("found signalfd file descriptor: %s\n", proc_fd_path);

        // construct /proc/<pid>/fdinfo/<fd>

        snprintf(fdinfo_path, PROC_PATH_MAX, "%s", proc_fdinfo_dir_path);
        strncat(fdinfo_path, "/", PROC_PATH_MAX - strlen(fdinfo_path) - 1);
        strncat(fdinfo_path, entry->d_name, PROC_PATH_MAX - strlen(fdinfo_path) - 1);
        fdinfo_path[PROC_PATH_MAX - 1] = 0;
        
        // read the fdinfo file; note: sizes of files /proc/<pid>/fdinfo vary; we
        // read one line at a time, so a buffer of 80 chars should be enough

        FILE *fdinfo = fopen(fdinfo_path, "r");
        char readbuf[80];

        // Here is what we expect to find in the file if it handles SIGHUP:
        //
        // pos:	0
        // flags:	02004002
        // mnt_id:	16
        // ino:	48
        // sigmask:	0000000000000001

        // go line-by-line to get the sigmask: line

        while (fgets(readbuf, 80, fdinfo) != NULL) {
            if (strncmp(readbuf, "sigmask:", 8) == 0) {
                // get rid of trailing \n
                if (strlen(readbuf) > 0) {
                    readbuf[strlen(readbuf) - 1] = 0;
                }
                unsigned long mask;
                char *p = &readbuf[0];
                while(!isspace(*p)) p++;
                while(isspace(*p)) p++;
                sscanf(p, "%lx", &mask);

                // if we found signalfd, reset it
                if (mask & (1 << (SIGHUP - 1))) {
                    sscanf(entry->d_name, "%d", &sighup_fd);
                    
                    // use the same fd to receive SIGUSR1; thus, labwc will henceforth
                    // reconfigure itself upon receiving SIGUSR1 instead of SIGHUP,
                    // without noticing the change

                    sigset_t usr1mask;
                    sigemptyset(&usr1mask);
                    sigaddset(&usr1mask, SIGUSR1);
                    signalfd(sighup_fd, &usr1mask, 0);
                    sigprocmask(SIG_BLOCK, &usr1mask, NULL);

                    // set up normal, asynchronous SIGHUP handling and pass our SIGHUP handler

                    sigset_t hupmask;
                    sigemptyset(&hupmask);
                    sigaddset(&hupmask, SIGHUP);
                    signal(SIGHUP, sighup_handler);
                    sigprocmask(SIG_UNBLOCK, &hupmask, NULL);
                }

                kbdd_log("found line %s, p=%s, mask=%lx (%sSIGHUP),\n\tsighup_fd=%d\n",
                        readbuf, p, mask, (mask & (1 << (SIGHUP - 1)))? "" : "not ", sighup_fd);
            }

            // exit the loop if we found what we were looking for

            if (sighup_fd != -1) break;
        }
        fclose(fdinfo);

        // exit the loop if we found what we were looking for
        if (sighup_fd != -1) break;
    }
    closedir(proc_fd_dir);

    // if no SIGHUP signalfd not found, flag as an error, so that we do not retry ever after
    // in that case, we fall back to checking the env variable XKB_DEFAULT_LAYOUT each time
    // (which is a bit more wasteful, but works)

    if (sighup_fd == -1) {
        kbdd_log("did not find SIGHUP signalfd, setting sighup_fd to -2\n");
        sighup_fd = -2;
    }

    return sighup_fd;
}
