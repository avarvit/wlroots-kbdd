#include <wlr/interfaces/wlr_keyboard.h>
#include <dlfcn.h>
#include <assert.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_seat.h>
#include "kbdd.h"

static struct wlr_seat *last_focused_seat = NULL;

static struct wlr_surface *last_focused_surface = NULL;
static void *last_focused_toplevel = NULL;
static struct wlr_surface *last_focused_toplevel_surface = NULL;
static int pending_group = -1;
static struct wlr_surface *pending_last_focused_toplevel_surface = NULL;

typedef void (*wlr_seat_keyboard_enter_type)(struct wlr_seat *,
                struct wlr_surface *, const uint32_t [], size_t,
                const struct wlr_keyboard_modifiers *);

void wlr_seat_keyboard_enter(struct wlr_seat *seat,
                struct wlr_surface *surface, const uint32_t keycodes[], size_t num_keycodes,
                const struct wlr_keyboard_modifiers *modifiers) {

    // wlr_seat = seat;

    static wlr_seat_keyboard_enter_type real_wlr_seat_keyboard_enter = NULL;

    // dlget real wlr_seat_keyboard_enter 
    if (real_wlr_seat_keyboard_enter == NULL) {
        real_wlr_seat_keyboard_enter = (wlr_seat_keyboard_enter_type) dlsym(RTLD_NEXT,
                    "wlr_seat_keyboard_enter");
        assert(real_wlr_seat_keyboard_enter != NULL);
    }

    if (surface) {
            struct wl_client *wl_client = wl_resource_get_client(surface->resource);
            kbdd_log("wlr_seat_keyboard_enter, surface=%p, wl_client %p\n", surface, wl_client);
    }

    last_focused_surface = surface;
    last_focused_seat = seat;

    // invoke the real thing
    (*real_wlr_seat_keyboard_enter)(seat,
                surface, keycodes, num_keycodes,
                modifiers);

    if (pending_group >= 0) {
        if (surface == pending_last_focused_toplevel_surface) {
            kbdd_log("at this point, I would have set group to %d\n", pending_group);
        }
        else {
            kbdd_log("after change, focused to different client, group change discarded\n");
        }
        pending_last_focused_toplevel_surface = NULL;
        pending_group = -1;
    }
}

void mark_focused_as_toplevel(void *toplevel) {
    last_focused_toplevel_surface = last_focused_surface;
    last_focused_toplevel = toplevel;
}

void unmark_focused_toplevel(void * toplevel) {
    if (toplevel == last_focused_toplevel) {
        last_focused_toplevel_surface = NULL;
        last_focused_toplevel = NULL;
    }
}

void handle_kbdd_layout_group(uint32_t group) {

    if (last_focused_seat == NULL || last_focused_toplevel == NULL
                || last_focused_toplevel_surface == NULL) {
        return;
    }

    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(last_focused_seat);
    if (keyboard == NULL) {
        kbdd_log("handle_kbdd_layout_group: null keyboard, event discarded\n");
        return;
    }

    // TODO: can we make sure that keyboard is not virtual?
    wlr_keyboard_notify_modifiers(keyboard, keyboard->modifiers.depressed,
                keyboard->modifiers.latched, keyboard->modifiers.locked,
                group);

    kbdd_log("handle_kbdd_layout_group: (group=%d), last_focused=%p\n",
                group, last_focused_surface);

    pending_group = group;
    pending_last_focused_toplevel_surface = last_focused_toplevel_surface;
}
