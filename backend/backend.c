#include <wayland-server-core.h>
#include <wlr/backend.h>
#include <assert.h>
#include <dlfcn.h>
#include "kbdd.h"

typedef struct wlr_backend *(*wlr_backend_autocreate_type)(struct wl_event_loop *, struct wlr_session **);

struct wlr_backend *wlr_backend_autocreate(struct wl_event_loop *loop,
                struct wlr_session **session_ptr) {

    static wlr_backend_autocreate_type real_wlr_backend_autocreate = NULL;

    // hijack the event loop
    kbdd_steal_event_loop(loop);

    // dlget the real wlr_backend_autocreate
    if (real_wlr_backend_autocreate == NULL) {
        real_wlr_backend_autocreate = (wlr_backend_autocreate_type) dlsym(RTLD_NEXT,
                    "wlr_backend_autocreate");
        assert(real_wlr_backend_autocreate != NULL);
    }

    // invoke the real thing
    return (*real_wlr_backend_autocreate)(loop, session_ptr);
}

