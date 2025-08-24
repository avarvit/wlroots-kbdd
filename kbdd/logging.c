#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include "kbdd.h"

#define LOG_PREFIX "wlroots_kbdd: "

static bool info_msg_printed = false;

void kbdd_log(const char *fmt, ...) {
    FILE *debug_file = NULL;
    const char *debug_path = getenv("DEBUG_WLROOTS_KBDD");
    if (debug_path == NULL) {
        return;
    }
    if (*debug_path != 0) {
        // only allow opening a debug file under /tmp
        if (strncmp(debug_path, "/tmp/", 4)) {
            fprintf(stderr, "DEBUG_WLROOTS_KBDD: invalid path %s, must start with /tmp/ "
            "- debugging to stderr instead\n", debug_path);
            setenv("DEBUG_WLROOTS_KBDD", "", 1);
            debug_file = stderr;
        }
        else {
            debug_file = fopen(debug_path, "a+");
        }
        if (debug_file == NULL) {
            int err = errno;
            fprintf(stderr, LOG_PREFIX
                        "failed to open %s for writing: %s; debugging to stderr instead\n",
                        debug_path, strerror(err));
            setenv("DEBUG_WLROOTS_KBDD", "", 1);
            debug_file = stderr;
        }
    }
    else {
        if (!info_msg_printed) {
            fprintf(stderr, LOG_PREFIX
                        "no path specified in DEBUG_WLROOTS_KBDD, debugging to stderr\n");
            info_msg_printed = true;
        }
        debug_file = stderr;
    }

    fprintf(stderr, LOG_PREFIX);
    va_list args;
    va_start(args, fmt);
    vfprintf(debug_file, fmt, args);
    va_end(args);

    if (debug_file != stderr) {
        fclose(debug_file);
    }
}

