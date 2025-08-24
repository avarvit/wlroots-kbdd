#ifndef KBDD_H
#define KBDD_H

#include <stdio.h>
#include <wayland-server-core.h>

#define GRATUITOUS_LAYOUT_SEND_MS 5000
#define DBUS_SERVER_POLL_EVERY_MS  500

struct kbdd_context {
    char *env_layouts;
    char **layout_short_names;
    uint32_t num_layouts;
    int prev_layout;
    bool expect_envchange;
    struct wl_event_loop *cmp_evt_loop;
    struct wl_event_source *client_timer;
    struct wl_event_source *server_timer;
};

//logging
void kbdd_log(const char *, ...);

void kbdd_handle_set_layout_group(int);
void kbdd_handle_set_keymap(void);
void kbdd_steal_event_loop(struct wl_event_loop *event_loop);

bool unsetenv_ourselves(void);

int initialize_sighup(struct kbdd_context *);

// dbus_client
void dbus_client_connect(void);
void dbus_client_send_to_kbdlayout(const char *);
void dbus_client_arm_timer(struct kbdd_context *);
void dbus_client_disarm_timer(struct kbdd_context *);
int dbus_client_gratuitous_send_layout(void *);

// dbus_server
void dbus_server_start(struct kbdd_context *);
void dbus_server_stop(struct kbdd_context *);

void mark_focused_as_toplevel(void *);
void unmark_focused_toplevel(void *);
void handle_kbdd_layout_group(uint32_t group);

#endif // KBDD_H

