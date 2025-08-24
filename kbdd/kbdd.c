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

#include <glib.h>
#include <gio/gio.h>

#include <wlr/backend.h>

static const char *default_layouts = "us";

static struct kbdd_context ctx = {
    .env_layouts = NULL,        // layout names as passed by environment (or "US" if no env set)
    .layout_short_names = NULL, // short names of configured layouts
    .num_layouts = 0,           // number of layouts
    .prev_layout = -1,          // previous layout index
    .expect_envchange = true,   // whether to check for a change in XKB_DEFAULT_LAYOUT
    .cmp_evt_loop = NULL,       // wayland compositor event loop (where timers live)
    .client_timer = NULL,       // labwc timer to send gratuitous updates over DBus
    .server_timer = NULL,       // labwc timer to check incoming dbus requests
};
static struct kbdd_context *context = &ctx;

// static char *env_layouts = NULL;
static char *layouts_wrk = NULL;

static void initialize_timer(void);

// the introspection XML of wf-panel-pi (unused, kept in to help the
// reader understand the DBus messages we are sending)

// flags whether we have removed our path from LD_PRELOAD
static bool unsetenv_ok = false;

// the file descriptor on which we receive SIGHUP (set up via signalfd(2))
static int sighup_fd = -1;

static bool timer_set = false;
static bool server_started = false;

// we keep the compositor's main event loop, in order to set up a timer
// for sending gratuitous layout updates

// static struct wl_event_loop *compositor_event_loop = NULL;

void kbdd_steal_event_loop(struct wl_event_loop *event_loop) {
    context->cmp_evt_loop = event_loop;
}

// called each time the layout group changes; updates wf-panel-pi via a DBus msg

void kbdd_handle_set_layout_group(int layout) {
    if (context->prev_layout != layout) {
        kbdd_log("layout changed from %d to %d\n", context->prev_layout, layout);
        context->prev_layout = layout;

        // inform wfpanel via DBus
        dbus_client_send_to_kbdlayout(context->layout_short_names[layout]);
    }
}


// kbdd_handle_set_keymap (further down) is called every time a keymap changes. This
// may happen multiple times, but for sure happens at least once during initialization,
// and every time a reconfiguration occurs that affects keymaps. With labwc, the
// keymap layouts are set via the environment variable XKB_DEFAULT_LAYOUT. We
// could check this environment variable each time we are called (actually, this
// is our fallback), but we try to be smarter and only check it if a reconfiguration
// is in progress (we know, because we intercept SIGHUP, see below).
//
// In addition, because this is called during the compositor initialization, we
// hook in a few more  initialization tasks here: (a) our gratuitous layout send
// timer initialization, (b) the SIGHUP hijacking logic (see below) and (c) the
// DBus initialization (also)


// initialize our client timer via the compositor's event loop (hijacked earlier)

static void initialize_timer(void) {
    if (context->cmp_evt_loop != NULL) {
        kbdd_log("setting client timer on compositor's event loop (%dms)\n",
                    GRATUITOUS_LAYOUT_SEND_MS);
        context->client_timer = wl_event_loop_add_timer(
                    context->cmp_evt_loop, dbus_client_gratuitous_send_layout, (void *)context);
        wl_event_source_timer_update(context->client_timer, GRATUITOUS_LAYOUT_SEND_MS);
        timer_set = true;
    }
    else {
        kbdd_log("compositor's event loop is null\n");
    }
    timer_set = true; // to avoid retrying if compositor_event_loop is for some reason NULL
}

void kbdd_handle_set_keymap(void) {

    char *layouts = NULL;
    int layouts_counter = 0;

    // remove ourselves from LD_PRELOAD
    if (!unsetenv_ok) {
        unsetenv_ok = unsetenv_ourselves();
    }

    // check if our timer is set; if not (it's our first time through) initialize it
    if (!timer_set) {
        initialize_timer();
    }

    if (sighup_fd == -1) {          // first time through, attempt to
                                    // intercept SIGHUP as explained
        sighup_fd = initialize_sighup(context);
    }
    else if (sighup_fd < 0) {       // if our previous attempt failed, don't bother retrying
        kbdd_log("sighup_fd=%d, skipping tests\n", sighup_fd);
    }

    // set up our dbus connection
    dbus_client_connect();

    // check if dbus server has started; if not (it's our first time through) start it
    if (!server_started) {
        dbus_server_start(context);
        server_started = true;
    }


    // assuming that our signal handling has been properly set up before,
    // we only check our environment via getenv() when it may have changed
    // (otherwise, we check each and every time, which is a bit wasteful,
    // but still functional)

    if (sighup_fd >= 0 && !context->expect_envchange) {
        kbdd_log("sighup_fd > 0 and expect_envchange is false, not checking environment\n");
        return;
    }

    kbdd_log("re-checking environment\n");
    layouts = getenv("XKB_DEFAULT_LAYOUT");
    context->expect_envchange = false;

    // make sure we have a layout and that is not empty
    if (!layouts || !*layouts) {
        kbdd_log("no layouts in environment, defaulting to \"%s\"\n", default_layouts);
        layouts = (char *) default_layouts;
    }

    // nothing to do if unchanged (note: env_layouts initializes as NULL)
    if (layouts == context->env_layouts) {
        return;
    }

    kbdd_log("kbdd_handle_set_keymap using new layouts setting %s\n", layouts);

    // make our own copy, so we can modify it
    if (layouts_wrk != NULL) {
        free(layouts_wrk);
        layouts_wrk = NULL;
    }
    layouts_wrk = strdup(layouts);

    kbdd_log("kbdd_handle_set_keymap handling %s\n", layouts_wrk);

    // discard old short_names array
    if (context->layout_short_names != NULL) {
        free(context->layout_short_names);
        context->layout_short_names = NULL;
    }

    // count layout short names
    layouts_counter = 1;
    for (char *p = layouts_wrk; *p; p++) {
        if (*p == ',') {
            layouts_counter++;
        }
    }

    kbdd_log("kbdd_handle_set_keymap counted %d layouts\n", layouts_counter);

    // reinitialize our short_names array
    assert (context->layout_short_names == NULL);
    context->layout_short_names = (char **) malloc (strlen(layouts_wrk) + 1);
    if (context->layout_short_names == NULL) {
        perror ("malloc failed");
        exit (1);
    }

    // loop over it again, changing commas to end-of-strings and pointing to
    // each short name, while at the same time capitalizing lnames
    context->layout_short_names[0] = layouts_wrk;
    layouts_counter = 1;
    for (char *p = layouts_wrk; *p; p++) {
        if (*p == ',') {
            *p = 0;
            kbdd_log("kbdd_handle_set_keymap layout_short_names[%d]=%s\n", layouts_counter - 1,
                        context->layout_short_names[layouts_counter - 1]);
            context->layout_short_names[layouts_counter] = p + 1;
            layouts_counter++;
        }
        else {
            // TODO: track if we are within parentheses (as in, e.g., us(intl)) and
            // if we are, skip capitalizion of the string in parentheses
            *p = toupper(*p);
        }
    }
    kbdd_log("kbdd_handle_set_keymap layout_short_names[%d]=%s\n", layouts_counter - 1,
                context->layout_short_names[layouts_counter - 1]);

    context->num_layouts = layouts_counter;

    // remember newly-set layouts string to avoid having to re-initialize every time
    context->env_layouts = layouts;

    kbdd_log("Found %d layout%s:\n", layouts_counter, (layouts_counter == 1)? "" : "s");
    for (int i = 0; i < layouts_counter; i++) {
        kbdd_log("%s\n", context->layout_short_names[i]);
    }

    dbus_client_send_to_kbdlayout(context->env_layouts);

    context->prev_layout = 0;
    dbus_client_send_to_kbdlayout(context->layout_short_names[0]);
}
