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

static GDBusConnection *dbus_conn = NULL;
static GError *dbus_error = NULL;

static const char *dbus_srvr_name = "org.wayfire.wfpanel";
static const char *dbus_objt_path = "/org/wayfire/wfpanel";
static const char *dbus_ifce_name = "org.wayfire.wfpanel";


// the introspection XML of wf-panel-pi (unused, kept in to help the
// reader understand the DBus messages we are sending

#if 0
const char *notify_introspection_xml=
"  <?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"  <node>\n"
"    <interface name=\"org.wayfire.wfpanel\">\n"
"      <!--\n"
"        This protocol emulates the function of wayfire's wfpanelctl utility\n"
"        to send \"commands\" to specific wfpanel widgets\n"
"      -->\n"
"      <method name=\"command\">\n"
"        <arg type=\"s\" name=\"widget\" direction=\"in\"/>\n"
"        <arg type=\"s\" name=\"argument\" direction=\"in\"/>\n"
"      </method>\n"
"  </node>\n"
;
#endif

void dbus_client_arm_timer(struct kbdd_context *context) {
    if (context->client_timer != NULL) {
        wl_event_source_timer_update(context->client_timer, GRATUITOUS_LAYOUT_SEND_MS);
    }
    else {
        kbdd_log("disarm_timer called while context->client_timer is null\n");
    }
}

void dbus_client_disarm_timer(struct kbdd_context *context) {
    if (context->client_timer != NULL) {
        wl_event_source_timer_update(context->client_timer, -1); // this disarms the timer
    }
    else {
        kbdd_log("disarm_timer called while context->client_timer is null\n");
    }
}

int dbus_client_gratuitous_send_layout(void *data) {

    struct kbdd_context *context = (struct kbdd_context *) data;

    // inform wfpanel via DBus
    if (dbus_conn != NULL && context->layout_short_names != NULL && context->prev_layout != -1) {
        dbus_error = NULL;
        GVariant *result = g_dbus_connection_call_sync(
            dbus_conn,                                  // connection
            dbus_srvr_name,                             // bus name
            dbus_objt_path,                             // object path
            dbus_ifce_name,                             // interface name
            "command",                                  // method
            g_variant_new("(ss)",
                 "kbdlayout",
                 context->layout_short_names[context->prev_layout]),      // args (auto-consumed if floating)
            NULL,                                       // reply type
            G_DBUS_CALL_FLAGS_NONE,                     // flags
            1,                                          // timeout in ms
            NULL,                                       // cancelable
            // for async version: GAsyncReadyCallback     callback,
            &dbus_error                                 // error
        );
        if (result != NULL) {
            g_variant_unref(result);
        }
    }

    // pick up timer from context and re-arm it
    wl_event_source_timer_update(context->client_timer, GRATUITOUS_LAYOUT_SEND_MS);

    return 0;
}

void dbus_client_send_to_kbdlayout(const char *args) {
    if (dbus_conn != NULL) {
        dbus_error = NULL;
        GVariant *result = g_dbus_connection_call_sync(
            dbus_conn,                                  // connection
            dbus_srvr_name,                             // bus name
            dbus_objt_path,                             // object path
            dbus_ifce_name,                             // interface name
            "command",                                  // method
            g_variant_new("(ss)",
                 "kbdlayout", args),                    // args (auto-consumed if floating)
            NULL,                                       // reply type
            G_DBUS_CALL_FLAGS_NONE,                     // flags
            10,                                         // timeout in ms
            NULL,                                       // cancelable
            // for async version: GAsyncReadyCallback     callback,
            &dbus_error                                 // error
        );
        if (result != NULL) {
            g_variant_unref(result);
        }
    }
}

void dbus_client_connect(void) {
    if (dbus_conn == NULL) {

        dbus_error = NULL;
        dbus_conn = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &dbus_error);
	g_assert_no_error(dbus_error);

        kbdd_log("dbus client connected to bus\n");
    }
}
