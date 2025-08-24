#include <wlr/types/wlr_foreign_toplevel_management_v1.h>
#include <dlfcn.h>
#include <assert.h>
#include "kbdd.h"


typedef void (*wlr_foreign_toplevel_handle_v1_set_activated_type)(
            struct wlr_foreign_toplevel_handle_v1 *,
            bool);

void wlr_foreign_toplevel_handle_v1_set_activated(
            struct wlr_foreign_toplevel_handle_v1 *toplevel,
            bool activated) {

    static wlr_foreign_toplevel_handle_v1_set_activated_type
                real_wlr_foreign_toplevel_handle_v1_set_activated= NULL;


    // dlget real wlr_foreign_toplevel_handle_v1_set_activated
    if (real_wlr_foreign_toplevel_handle_v1_set_activated == NULL) {
        real_wlr_foreign_toplevel_handle_v1_set_activated =
                    (wlr_foreign_toplevel_handle_v1_set_activated_type) dlsym(RTLD_NEXT,
                    "wlr_foreign_toplevel_handle_v1_set_activated");
        assert(real_wlr_foreign_toplevel_handle_v1_set_activated != NULL);
    }

    kbdd_log("%sactivating toplevel %p with %stitle %s\n",
                activated? "":"de", toplevel, toplevel->title? "":"no ",
                toplevel->title? toplevel->title: "");

    if (activated) {
        mark_focused_as_toplevel((void *)toplevel);
    }
    else {
        unmark_focused_toplevel((void *)toplevel);
    }
    
    // invoke the real thing
    (*real_wlr_foreign_toplevel_handle_v1_set_activated )(toplevel, activated);
}


typedef void (*wlr_foreign_toplevel_handle_v1_destroy_type)(
            struct wlr_foreign_toplevel_handle_v1 *);

void wlr_foreign_toplevel_handle_v1_destroy(
            struct wlr_foreign_toplevel_handle_v1 *toplevel) {

    static wlr_foreign_toplevel_handle_v1_destroy_type
            real_wlr_foreign_toplevel_handle_v1_destroy = NULL;

    // dleget real wlr_foreign_toplevel_handle_v1_destroy
    if (real_wlr_foreign_toplevel_handle_v1_destroy == NULL) {
        real_wlr_foreign_toplevel_handle_v1_destroy =
                    (wlr_foreign_toplevel_handle_v1_destroy_type) dlsym(RTLD_NEXT,
                    "wlr_foreign_toplevel_handle_v1_destroy");
        assert(real_wlr_foreign_toplevel_handle_v1_destroy != NULL);
    }

    kbdd_log("destroying toplevel %p with %stitle %s\n",
                toplevel, toplevel->title? "":"no ",
                toplevel->title? toplevel->title: "");

    unmark_focused_toplevel((void *)toplevel);

    // invoke the real thing
    (*real_wlr_foreign_toplevel_handle_v1_destroy)(toplevel);
}

