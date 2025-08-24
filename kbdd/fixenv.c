#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>
#include <sys/socket.h>
#include <sys/signalfd.h>
#include <errno.h>
#include <ctype.h>
#include <assert.h>
#include <dlfcn.h>
#include "kbdd.h"

#include <wlr/backend.h>

// remove our path from LD_PRELOAD and update environment, by either
// unsetenv()ing LD_PRELOAD if it is just us, or by re-setenv()ing
// it with its other libraries

bool unsetenv_ourselves(void) {
    kbdd_log("removing ourselves from LD_PRELOAD\n");

    // done if there is no LD_PRELOAD at all
    char *ld_preload = getenv("LD_PRELOAD");
    if (ld_preload == NULL) {
        return true;
    }

    // we do not know for sure our library's path, let us find out
    Dl_info our_dlinfo;
    void *our_wlr_backend_autocreate = (void *) wlr_backend_autocreate;
    if (dladdr(our_wlr_backend_autocreate, &our_dlinfo) == 0) {
        kbdd_log("could not dladdr() our wlr_backend_autocreate!\n");
        return true;
    }
    else {
        kbdd_log("our wlr_backend_autocreate is in \"%s\"\n", our_dlinfo.dli_fname);
    }
    const char *our_path = our_dlinfo.dli_fname;

    // if we are the only library in LD_PRELOAD, unsetenv it altogether
    if (!strcmp(ld_preload, our_path)) {
        kbdd_log("we are the only library in LD_PRELOAD, removing it from environment\n");
        unsetenv("LD_PRELOAD");
        return true;
    }
    else {
        kbdd_log("LD_PRELOAD contains other stuff too, doing a little surgery to remove us\n");
    }

    // create a new string to replace LD_PRELOAD, copying every other path but ours into it
    size_t ld_preload_length = strlen(ld_preload) + 1;
    char *new_ld_preload = (char *)malloc(ld_preload_length);
    memset(new_ld_preload, 0, ld_preload_length);
    char *saveptr = NULL;
    char *lib = strtok_r(ld_preload, ":", &saveptr);
    char *append = new_ld_preload;
    char *colon = NULL;
    while (lib != NULL) {
        kbdd_log("lib=%s, ld_preload_length=%lu\n", lib, ld_preload_length);
        if (strcmp(lib, our_path)) {
            if (colon != NULL) {
                *append++ = *colon;
                ld_preload_length--;
            }
            strncpy(append, lib, ld_preload_length);
            append += strlen(lib);
            ld_preload_length -= strlen(lib);
            colon = ":";

            kbdd_log("new_ld_preload now is %s\n", new_ld_preload);
        }
        else {
            ld_preload_length -= strlen(lib);
        }


        lib = strtok_r(NULL, ":", &saveptr);

        if (ld_preload_length <= 0) {
            kbdd_log("ld_preload_length consumed!\n");
            break;
        }
    }
    kbdd_log("new_ld_preload finally becomes %s\n", new_ld_preload);
    setenv("LD_PRELOAD", new_ld_preload, 1);
    free(new_ld_preload);

    return true;
}
