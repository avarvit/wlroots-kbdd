#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <signal.h>
#include <wayland-client.h>
#include "kbdd.h"

#include <glib.h>
#include <gio/gio.h>

static const char *KBDD_DBUS_SRV_PATH = "/org/wayfire/kbdd/layout";
static const char *KBDD_DBUS_SRV_NAME = "org.wayfire.kbdd.layout";
// static const char *KBDD_DBUS_SRV_IFCE = "org.wayfire.kbdd.layout";

extern const char *kbdd_srv_introspection;

static GDBusNodeInfo *dbus_introspection_data = NULL;
static GDBusConnection *dbus_connection = NULL;
static GError *dbus_error = NULL;
static GMainLoop *dbus_loop = NULL;
static int dbus_name_owned = -1;
static int dbus_obj_reg_id = -1;

static void handle_method_call (
            GDBusConnection       *,
            const gchar           *,
            const gchar           *,
            const gchar           *,
            const gchar           *,
            GVariant              *,
            GDBusMethodInvocation *,
            gpointer);

static GVariant *handle_get_property (
            GDBusConnection  *,
            const gchar      *,
            const gchar      *,
            const gchar      *,
            const gchar      *,
            GError          **,
            gpointer);

static gboolean handle_set_property (
            GDBusConnection  *,
            const gchar      *,
            const gchar      *,
            const gchar      *,
            const gchar      *,
            GVariant         *,
            GError          **,
            gpointer);

static const GDBusInterfaceVTable interface_vtable =
{
    handle_method_call,
    handle_get_property,
    handle_set_property,
    { 0 }
};

static void reset_dbus_error(void) {
    if (dbus_error != NULL) {
        g_error_free(dbus_error);
        dbus_error = NULL;
    }
}

static void name_acquired_handler(
            GDBusConnection *connection,
            const gchar     *name,
            gpointer         user_data) {

    kbdd_log("name=%s acquired!\n", name);

    // no real work to do
}

static void name_lost_handler(
        GDBusConnection *connection,
        const gchar     *name,
        gpointer         user_data) {

    
    kbdd_log("name=%s lost!\n", name);

    struct kbdd_context *context = (struct kbdd_context *)user_data;
    dbus_server_stop(context);
}

static void dbus_acquired_handler(
            GDBusConnection *conn,
            const gchar* name,
            gpointer user_data) {

    kbdd_log("bus_acquired_handler called, name=%s, saving connection\n", name);

    dbus_connection = conn;

    struct kbdd_context *context = (struct kbdd_context *) user_data;

    reset_dbus_error();
    dbus_introspection_data = g_dbus_node_info_new_for_xml(
                kbdd_srv_introspection,
                &dbus_error
    );
    if (dbus_introspection_data == NULL) {
        kbdd_log("g_dbus_node_info_new_for_xml error %s\n", dbus_error->message);

        dbus_server_stop(context);
        return;
    }

    dbus_obj_reg_id = g_dbus_connection_register_object(
                dbus_connection,
                KBDD_DBUS_SRV_PATH,
                dbus_introspection_data->interfaces[0],
                &interface_vtable,
                context, // user_data is our kbdd_context
                NULL,  // user_data_free_func: we are never to free ourselves context data
                &dbus_error
    );
    if (dbus_obj_reg_id == 0) {
        kbdd_log("g_dbus_connection_register_object error %s\n", dbus_error->message);

        dbus_server_stop(context);
        return;
    }
}


static GVariant *handle_get_property (
            GDBusConnection  *connection,
            const gchar      *sender,
            const gchar      *object_path,
            const gchar      *interface_name,
            const gchar      *property_name,
            GError          **error,
            gpointer          user_data) {

    return NULL;
}


static void handle_method_call (
            GDBusConnection       *connection,
            const gchar           *sender,
            const gchar           *object_path,
            const gchar           *interface_name,
            const gchar           *method_name,
            GVariant              *parameters,
            GDBusMethodInvocation *invocation,
            gpointer               user_data) {

    struct kbdd_context *context = (struct kbdd_context *)user_data;

    if (!strcmp(method_name, "enable")) {
        uint32_t turnon = -1;
        g_variant_get(parameters, "(u)", &turnon);

        kbdd_log("kbdd_dbus_server: method %s called with argument %u\n", method_name, turnon);

        if (turnon) {
            // our gratuitous timer-based send will do this for us
            // dbus_client_send_to_kbdlayout(context->layout_short_names[context->prev_layout]);

            dbus_client_send_to_kbdlayout(context->env_layouts);
            // ?? remove this?
            // dbus_client_arm_timer(context);
        }
        else {
            // ?? remove this?
            // dbus_client_disarm_timer(context);
            dbus_client_send_to_kbdlayout("-");
        }
    }
    else if (!strcmp(method_name, "switch")) {
        const char *layout_name = NULL;
        g_variant_get(parameters, "(&s)", &layout_name);

        kbdd_log("kbdd_dbus_server: method %s called with argument %s\n", method_name, layout_name);

        uint32_t group;
        for (group = 0; group < context->num_layouts; group++) {
            
            if (!strcmp(context->layout_short_names[group], layout_name)) {
                break;
            }
        }
        if (group < context->num_layouts) {
            kbdd_log("kbdd_dbus_server: %s matched as layout #%d\n", layout_name, group);
            handle_kbdd_layout_group(group);
        }
        else {
            kbdd_log("kbdd_dbus_server: %s did not match any of our layouts\n", layout_name);
        }
    }
    else {
        kbdd_log("kbdd_dbus_server: unknown method %s\n", method_name);
    }
}

static gboolean handle_set_property (
            GDBusConnection  *connection,
            const gchar      *sender,
            const gchar      *object_path,
            const gchar      *interface_name,
            const gchar      *property_name,
            GVariant         *value,
            GError          **error,
            gpointer          user_data) {

    return false;
}

static int dbus_poll(void *data) {

    struct kbdd_context *context = (struct kbdd_context *)data;
    // here: poll dbus
    if (dbus_loop != NULL) {
        g_main_context_iteration(g_main_loop_get_context(dbus_loop), FALSE);
    }

    // pick up timer from context and re-arm it
    wl_event_source_timer_update(context->server_timer, DBUS_SERVER_POLL_EVERY_MS);

    return 0;
}

void dbus_server_start(struct kbdd_context *context) {

    kbdd_log("starting dbus server\n");

    // set up the bus
    dbus_name_owned = g_bus_own_name(
                G_BUS_TYPE_SESSION,
                KBDD_DBUS_SRV_NAME,
                G_BUS_NAME_OWNER_FLAGS_DO_NOT_QUEUE,
                dbus_acquired_handler,
                name_acquired_handler,
                name_lost_handler,
                context, // user_data
                NULL  // user_data_free_func
    );
    if (dbus_name_owned == 0) {
        kbdd_log("error owning name\n");
    }
    else {
        kbdd_log("name %s owned successfully\n", KBDD_DBUS_SRV_NAME);
    }

    // create a main loop
    dbus_loop = g_main_loop_new (NULL, FALSE);
    if (dbus_loop == NULL) {
        kbdd_log("failed to create GDBus main loop\n");

        dbus_server_stop(context);
    }
    else {
        kbdd_log("GDBus main loop created successfully\n");
    }

    // set up server poll timer
    if (context->cmp_evt_loop != NULL) {
        kbdd_log("setting client timer on compositor's event loop (%dms)\n",
                    DBUS_SERVER_POLL_EVERY_MS);
        context->server_timer = wl_event_loop_add_timer(
                    context->cmp_evt_loop, dbus_poll, (void *)context);
        wl_event_source_timer_update(context->server_timer, DBUS_SERVER_POLL_EVERY_MS);

    }
}


void dbus_server_stop(struct kbdd_context *context) {
//unref here

    kbdd_log("kbdd_dbus_server: stopping dbus server\n");

    reset_dbus_error();
    // stop timer
    if (dbus_loop != 0) {
        g_main_loop_unref(dbus_loop);
        dbus_loop = NULL;
    }
    if (dbus_connection != NULL && dbus_obj_reg_id > 0) {
        g_dbus_connection_unregister_object(dbus_connection, dbus_obj_reg_id);
        dbus_obj_reg_id = -1;
    }
    if (dbus_name_owned > 0) {
        g_bus_unown_name(dbus_name_owned);
        dbus_connection = NULL;
        dbus_name_owned = -1;
    }
    if (dbus_introspection_data != NULL) {
        g_dbus_node_info_unref(dbus_introspection_data);
        dbus_introspection_data = NULL;
    }
}

