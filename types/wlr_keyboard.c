#include <wlr/interfaces/wlr_keyboard.h>
#include <dlfcn.h>
#include <assert.h>
#include <wayland-server-core.h>
#include "kbdd.h"

static xkb_layout_index_t prev_group = -1;

typedef void (*wlr_keyboard_notify_modifiers_type)(struct wlr_keyboard *,
            uint32_t, uint32_t, uint32_t, uint32_t);

void wlr_keyboard_notify_modifiers(struct wlr_keyboard *keyboard,
            uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked,
            uint32_t group) {

    static wlr_keyboard_notify_modifiers_type real_wlr_keyboard_notify_modifiers = NULL;

    // dlget real wlr_keyboard_notify_modifiers
    if (real_wlr_keyboard_notify_modifiers == NULL) {
        real_wlr_keyboard_notify_modifiers = (wlr_keyboard_notify_modifiers_type) dlsym(RTLD_NEXT,
                    "wlr_keyboard_notify_modifiers");
        assert(real_wlr_keyboard_notify_modifiers != NULL);
    }

    // invoke the real thing
    (*real_wlr_keyboard_notify_modifiers)(keyboard, mods_depressed, mods_latched, mods_locked, group);

    // check the updated layout group 
    if (keyboard->xkb_state == NULL) {
        return;
    }
    /*
    xkb_layout_index_t new_group = xkb_state_serialize_layout(keyboard->xkb_state,
                XKB_STATE_LAYOUT_EFFECTIVE);
    */

    // the real wlr_keyboard_notify_modifiers has updated keyboard->modifiers.group for us
    xkb_layout_index_t new_group = keyboard->modifiers.group;
    if (new_group != prev_group) {
        prev_group = new_group;
        kbdd_handle_set_layout_group(new_group);
    }
}

typedef bool (*wlr_keyboard_set_keymap_type)(struct wlr_keyboard *, struct xkb_keymap *);

bool wlr_keyboard_set_keymap(struct wlr_keyboard *keyboard, struct xkb_keymap *keymap) {
    static wlr_keyboard_set_keymap_type real_wlr_keyboard_set_keymap = NULL;

    // dlget real wlr_keyboard_set_keymap
    if (real_wlr_keyboard_set_keymap == NULL) {
        real_wlr_keyboard_set_keymap = (wlr_keyboard_set_keymap_type) dlsym(RTLD_NEXT,
                    "wlr_keyboard_set_keymap");
        assert(real_wlr_keyboard_set_keymap != NULL);
    }

    // first update our own internal state
    kbdd_handle_set_keymap();

    // then invoke the real thing
    return (*real_wlr_keyboard_set_keymap)(keyboard, keymap);
}
