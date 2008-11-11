/* vim:set et sts=4: */
/* ibus - The Input Bus
 * Copyright (C) 2008-2009 Huang Peng <shawn.p.huang@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <ibusinternal.h>
#include <ibusmarshalers.h>
#include "ibusimpl.h"
#include "inputcontext.h"
#include "engineproxy.h"
#include "factoryproxy.h"

#define BUS_INPUT_CONTEXT_GET_PRIVATE(o)  \
   (G_TYPE_INSTANCE_GET_PRIVATE ((o), BUS_TYPE_INPUT_CONTEXT, BusInputContextPrivate))

enum {
    SET_CURSOR_LOCATION,
    FOCUS_IN,
    FOCUS_OUT,
    UPDATE_PREEDIT,
    SHOW_PREEDIT,
    HIDE_PREEDIT,
    UPDATE_AUX_STRING,
    SHOW_AUX_STRING,
    HIDE_AUX_STRING,
    UPDATE_LOOKUP_TABLE,
    SHOW_LOOKUP_TABLE,
    HIDE_LOOKUP_TABLE,
    PAGE_UP_LOOKUP_TABLE,
    PAGE_DOWN_LOOKUP_TABLE,
    CURSOR_UP_LOOKUP_TABLE,
    CURSOR_DOWN_LOOKUP_TABLE,
    REGISTER_PROPERTIES,
    UPDATE_PROPERTY,
    LAST_SIGNAL,
};

enum {
    PROP_0,
};


/* IBusInputContextPriv */
struct _BusInputContextPrivate {
    BusConnection *connection;
    BusFactoryProxy *factory;
    BusEngineProxy *engine;
    gchar *client;
    
    gboolean has_focus;
    gboolean enabled;

    /* capabilities */
    guint capabilities;

    /* cursor location */
    gint x;
    gint y;
    gint w;
    gint h;
};

typedef struct _BusInputContextPrivate BusInputContextPrivate;

static guint    context_signals[LAST_SIGNAL] = { 0 };

/* functions prototype */
static void     bus_input_context_class_init    (BusInputContextClass   *klass);
static void     bus_input_context_init          (BusInputContext        *context);
static void     bus_input_context_destroy       (BusInputContext        *context);
static gboolean bus_input_context_ibus_message  (BusInputContext        *context,
                                                 BusConnection          *connection,
                                                 IBusMessage            *message);
static gboolean bus_input_context_filter_keyboard_shortcuts
                                                (BusInputContext        *context,
                                                 guint                   keyval,
                                                 gboolean                is_press,
                                                 guint                   state);
                                                 
static void     _factory_destroy_cb             (BusFactoryProxy        *factory,
                                                 BusInputContext        *context);

static IBusServiceClass  *_parent_class = NULL;
static guint id = 0;

GType
bus_input_context_get_type (void)
{
    static GType type = 0;

    static const GTypeInfo type_info = {
        sizeof (BusInputContextClass),
        (GBaseInitFunc)     NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc)    bus_input_context_class_init,
        NULL,               /* class finalize */
        NULL,               /* class data */
        sizeof (BusInputContext),
        0,
        (GInstanceInitFunc) bus_input_context_init,
    };

    if (type == 0) {
        type = g_type_register_static (IBUS_TYPE_SERVICE,
                    "BusInputContext",
                    &type_info,
                    (GTypeFlags) 0);
    }
    return type;
}

static void
_connection_destroy_cb (BusConnection   *connection,
                        BusInputContext *context)
{
    BUS_IS_CONNECTION (connection);
    BUS_IS_INPUT_CONTEXT (context);

    ibus_object_destroy (IBUS_OBJECT (context));
}


BusInputContext *
bus_input_context_new (BusConnection    *connection,
                       const gchar      *client)
{
    g_assert (BUS_IS_CONNECTION (connection));
    g_assert (client != NULL);

    BusInputContext *context;
    gchar *path;
    BusInputContextPrivate *priv;

    path = g_strdup_printf (IBUS_PATH_INPUT_CONTEXT, ++id);

    context = BUS_INPUT_CONTEXT (g_object_new (BUS_TYPE_INPUT_CONTEXT,
                    "path", path,
                    NULL));
    g_free (path);    
    
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    ibus_service_add_to_connection (IBUS_SERVICE (context),
                                 IBUS_CONNECTION (connection));

    g_object_ref (connection);
    priv->connection = connection;
    priv->client = g_strdup (client);

    g_signal_connect (priv->connection,
                      "destroy",
                      (GCallback) _connection_destroy_cb,
                      context);

    return context;
}

static void
bus_input_context_class_init (BusInputContextClass *klass)
{
    IBusObjectClass *ibus_object_class = IBUS_OBJECT_CLASS (klass);

    _parent_class = (IBusServiceClass *) g_type_class_peek_parent (klass);

    g_type_class_add_private (klass, sizeof (BusInputContextPrivate));

    ibus_object_class->destroy = (IBusObjectDestroyFunc) bus_input_context_destroy;

    IBUS_SERVICE_CLASS (klass)->ibus_message = (ServiceIBusMessageFunc) bus_input_context_ibus_message;

    /* install signals */
    context_signals[SET_CURSOR_LOCATION] =
        g_signal_new (I_("set-cursor-location"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__INT_INT_INT_INT,
            G_TYPE_NONE,
            4,
            G_TYPE_INT,
            G_TYPE_INT,
            G_TYPE_INT,
            G_TYPE_INT);

    context_signals[FOCUS_IN] =
        g_signal_new (I_("focus-in"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[FOCUS_OUT] =
        g_signal_new (I_("focus-out"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[UPDATE_PREEDIT] =
        g_signal_new (I_("update-preedit"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__STRING_BOXED_INT_BOOLEAN,
            G_TYPE_NONE,
            4,
            G_TYPE_STRING,
            IBUS_TYPE_ATTR_LIST | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_INT,
            G_TYPE_BOOLEAN);
    
    context_signals[SHOW_PREEDIT] =
        g_signal_new (I_("show-preedit"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
    
    context_signals[HIDE_PREEDIT] =
        g_signal_new (I_("hide-preedit"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
    
    context_signals[UPDATE_AUX_STRING] =
        g_signal_new (I_("update-aux-string"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__STRING_BOXED_BOOLEAN,
            G_TYPE_NONE,
            3,
            G_TYPE_STRING,
            IBUS_TYPE_ATTR_LIST | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_BOOLEAN);
    
    context_signals[SHOW_AUX_STRING] =
        g_signal_new (I_("show-aux-string"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
    
    context_signals[HIDE_AUX_STRING] =
        g_signal_new (I_("hide-aux-string"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);

    context_signals[UPDATE_LOOKUP_TABLE] =
        g_signal_new (I_("update-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__BOXED_BOOLEAN,
            G_TYPE_NONE,
            2,
            IBUS_TYPE_LOOKUP_TABLE | G_SIGNAL_TYPE_STATIC_SCOPE,
            G_TYPE_BOOLEAN);
    
    context_signals[SHOW_LOOKUP_TABLE] =
        g_signal_new (I_("show-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[HIDE_LOOKUP_TABLE] =
        g_signal_new (I_("hide-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);

    context_signals[PAGE_UP_LOOKUP_TABLE] =
        g_signal_new (I_("page-up-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[PAGE_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("page-down-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[CURSOR_UP_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-up-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[CURSOR_DOWN_LOOKUP_TABLE] =
        g_signal_new (I_("cursor-down-lookup-table"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
    
    context_signals[REGISTER_PROPERTIES] =
        g_signal_new (I_("register-properties"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__BOXED,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROP_LIST | G_SIGNAL_TYPE_STATIC_SCOPE);
    
    context_signals[UPDATE_PROPERTY] =
        g_signal_new (I_("update-property"),
            G_TYPE_FROM_CLASS (klass),
            G_SIGNAL_RUN_LAST,
            0,
            NULL, NULL,
            ibus_marshal_VOID__BOXED,
            G_TYPE_NONE,
            1,
            IBUS_TYPE_PROPERTY | G_SIGNAL_TYPE_STATIC_SCOPE);

}

static void
bus_input_context_init (BusInputContext *context)
{
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    priv->connection = NULL;
    priv->client = NULL;
    priv->factory = NULL;
    priv->engine = NULL;
    priv->has_focus = FALSE;
    priv->enabled = FALSE;

    priv->capabilities = 0;

    priv->x = 0;
    priv->y = 0;
    priv->w = 0;
    priv->h = 0;
}

static void
bus_input_context_destroy (BusInputContext *context)
{
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    if (priv->factory) {
        g_signal_handlers_disconnect_by_func (priv->factory,
                                              G_CALLBACK (_factory_destroy_cb),
                                              context);
        g_object_unref (priv->factory);
        priv->factory = NULL;
    }

    if (priv->engine) {
        ibus_object_destroy (IBUS_OBJECT (priv->engine));
        priv->engine = NULL;
    }

    if (priv->connection) {
        g_signal_handlers_disconnect_by_func (priv->connection,
                                         (GCallback) _connection_destroy_cb,
                                         context);
        g_object_unref (priv->connection);
        priv->connection = NULL;
    }

    if (priv->client) {
        g_free (priv->client);
        priv->client = NULL;
    }

    IBUS_OBJECT_CLASS(_parent_class)->destroy (IBUS_OBJECT (context));
}

/* introspectable interface */
static IBusMessage *
_ibus_introspect (BusInputContext   *context,
                  IBusMessage       *message,
                  BusConnection     *connection)
{
    static const gchar *introspect =
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
        "<node>\n"
        "  <interface name=\"org.freedesktop.DBus.Introspectable\">\n"
        "    <method name=\"Introspect\">\n"
        "      <arg name=\"data\" direction=\"out\" type=\"s\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "  <interface name=\"org.freedesktop.IBus\">\n"
        "    <method name=\"RequestName\">\n"
        "      <arg direction=\"in\" type=\"s\"/>\n"
        "      <arg direction=\"in\" type=\"u\"/>\n"
        "      <arg direction=\"out\" type=\"u\"/>\n"
        "    </method>\n"
        "    <signal name=\"NameOwnerChanged\">\n"
        "      <arg type=\"s\"/>\n"
        "      <arg type=\"s\"/>\n"
        "      <arg type=\"s\"/>\n"
        "    </signal>\n"
        "  </interface>\n"
        "</node>\n";

    IBusMessage *reply_message;
    reply_message = ibus_message_new_method_return (message);
    ibus_message_append_args (reply_message,
                              G_TYPE_STRING, &introspect,
                              G_TYPE_INVALID);

    return reply_message;
}

static IBusMessage *
_ic_process_key_event (BusInputContext  *context,
                       IBusMessage      *message,
                       BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));

    IBusMessage *reply;
    guint32 keyval, state;
    gboolean is_press;
    gboolean retval;
    IBusError *error;
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);
    

    error = ibus_error_new ();
    retval = ibus_message_get_args (message,
                &error,
                G_TYPE_UINT, &keyval,
                G_TYPE_BOOLEAN, &is_press,
                G_TYPE_UINT, &state,
                G_TYPE_INVALID);

    if (!retval) {
        reply = ibus_message_new_error (message,
                                        error->name,
                                        error->message);
        ibus_error_free (error);
        return reply;
    }
    
    ibus_error_free (error);
    
    retval = bus_input_context_filter_keyboard_shortcuts (context,
                                                          keyval,
                                                          is_press,
                                                          state);
    if (retval)
        goto out;

    if (priv->enabled && priv->engine) {
        retval = bus_engine_proxy_process_key_event (priv->engine,
                                                     keyval,
                                                     is_press,
                                                     state);
    }
    else {
        retval = FALSE;
    }

out:
    reply = ibus_message_new_method_return (message);
    ibus_message_append_args (reply,
                              G_TYPE_BOOLEAN, &retval,
                              G_TYPE_INVALID);

    return reply;
}

static IBusMessage *
_ic_set_cursor_location (BusInputContext  *context,
                         IBusMessage      *message,
                         BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));

    IBusMessage *reply;
    guint32 x, y, w, h;
    gboolean retval;
    IBusError *error;

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);
    
    retval = ibus_message_get_args (message, &error,
                G_TYPE_UINT, &x,
                G_TYPE_UINT, &y,
                G_TYPE_UINT, &w,
                G_TYPE_UINT, &h,
                G_TYPE_INVALID);

    if (!retval) {
        reply = ibus_message_new_error (message,
                                        error->name,
                                        error->message);
        ibus_error_free (error);
        return reply;
    }

    priv->x = x;
    priv->y = y;
    priv->h = h;
    priv->w = w;
    
    if (priv->engine) {
        bus_engine_proxy_set_cursor_location (priv->engine, x, y, w, h);
    }

    if (priv->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context,
                       context_signals[SET_CURSOR_LOCATION],
                       0,
                       x,
                       y,
                       w,
                       h);
    }

    reply = ibus_message_new_method_return (message);
    return reply;
}

static IBusMessage *
_ic_focus_in (BusInputContext  *context,
              IBusMessage      *message,
              BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));

    IBusMessage *reply;
    
    bus_input_context_focus_in (context);

    reply = ibus_message_new_method_return (message);

    return reply;
}

static IBusMessage *
_ic_focus_out (BusInputContext  *context,
              IBusMessage      *message,
              BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));

    IBusMessage *reply;

    bus_input_context_focus_out (context);
    
    reply = ibus_message_new_method_return (message);

    return reply;
}

static IBusMessage *
_ic_reset (BusInputContext  *context,
           IBusMessage      *message,
           BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));

    IBusMessage *reply;
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);
    
    
    if (priv->engine) {
        bus_engine_proxy_reset (priv->engine);
    }

    reply = ibus_message_new_method_return (message);
    return reply;
}

static IBusMessage *
_ic_set_capabilities (BusInputContext  *context,
                      IBusMessage      *message,
                      BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));
    
    IBusMessage *reply;
    guint32 caps;
    gboolean retval;
    IBusError *error;
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);
    

    retval = ibus_message_get_args (message,
                &error,
                G_TYPE_UINT, &caps,
                G_TYPE_INVALID);

    if (!retval) {
        reply = ibus_message_new_error (message,
                                        error->name,
                                        error->message);
        ibus_error_free (error);
        return reply;
    }

    if (priv->capabilities != caps) {
        priv->capabilities = caps;
        
        if (priv->engine) {
            bus_engine_proxy_set_capabilities (priv->engine, caps);
        }
    }

    reply = ibus_message_new_method_return (message);
    return reply;
}

static IBusMessage *
_ic_is_enabled (BusInputContext  *context,
                IBusMessage      *message,
                BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));
    
    IBusMessage *reply;
    gboolean retval;
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    reply = ibus_message_new_method_return (message);
    ibus_message_append_args (reply,
            G_TYPE_BOOLEAN, &priv->enabled,
            G_TYPE_INVALID);

    return reply;
}

static IBusMessage *
_ic_get_factory (BusInputContext  *context,
                 IBusMessage      *message,
                 BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));
    
    IBusMessage *reply;
    const gchar *factory = "";
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    if (priv->factory) {
        factory = ibus_proxy_get_path (IBUS_PROXY (priv->factory));
    }

    reply = ibus_message_new_method_return (message);
    ibus_message_append_args (reply,
            G_TYPE_STRING, &factory,
            G_TYPE_INVALID);

    return reply;
}

static IBusMessage *
_ic_destroy (BusInputContext  *context,
             IBusMessage      *message,
             BusConnection    *connection)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (message != NULL);
    g_assert (BUS_IS_CONNECTION (connection));
    
    IBusMessage *reply;
    
    ibus_object_destroy (IBUS_OBJECT (context));
    reply = ibus_message_new_method_return (message);
    
    return reply;
}

static gboolean
bus_input_context_ibus_message (BusInputContext *context,
                                BusConnection   *connection,
                                IBusMessage     *message)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    g_assert (BUS_IS_CONNECTION (connection));
    g_assert (message != NULL);

    gint i;
    IBusMessage *reply_message = NULL;

    struct {
        const gchar *interface;
        const gchar *name;
        IBusMessage *(* handler) (BusInputContext *, IBusMessage *, BusConnection *);
    } handlers[] =  {
        /* Introspectable interface */
        { DBUS_INTERFACE_INTROSPECTABLE,
                               "Introspect", _ibus_introspect },
        /* IBus interface */
        { IBUS_INTERFACE_INPUT_CONTEXT, "ProcessKeyEvent",   _ic_process_key_event },
        { IBUS_INTERFACE_INPUT_CONTEXT, "SetCursorLocation", _ic_set_cursor_location },
        { IBUS_INTERFACE_INPUT_CONTEXT, "FocusIn",           _ic_focus_in },
        { IBUS_INTERFACE_INPUT_CONTEXT, "FocusOut",          _ic_focus_out },
        { IBUS_INTERFACE_INPUT_CONTEXT, "Reset",             _ic_reset },
        { IBUS_INTERFACE_INPUT_CONTEXT, "SetCapabilities",   _ic_set_capabilities },
        { IBUS_INTERFACE_INPUT_CONTEXT, "IsEnabled",         _ic_is_enabled },
        { IBUS_INTERFACE_INPUT_CONTEXT, "GetFactory",        _ic_get_factory },
        { IBUS_INTERFACE_INPUT_CONTEXT, "Destroy",           _ic_destroy },
        
        { NULL, NULL, NULL }
    };

    ibus_message_set_sender (message, bus_connection_get_unique_name (connection));
    ibus_message_set_destination (message, DBUS_SERVICE_DBUS);

    for (i = 0; handlers[i].interface != NULL; i++) {
        if (ibus_message_is_method_call (message,
                                         handlers[i].interface,
                                         handlers[i].name)) {

            reply_message = handlers[i].handler (context, message, connection);
            if (reply_message) {

                ibus_message_set_sender (reply_message,
                                         DBUS_SERVICE_DBUS);
                ibus_message_set_destination (reply_message,
                                              bus_connection_get_unique_name (connection));
                ibus_message_set_no_reply (reply_message, TRUE);

                ibus_connection_send (IBUS_CONNECTION (connection), reply_message);
                ibus_message_unref (reply_message);
            }

            g_signal_stop_emission_by_name (context, "ibus-message");
            return TRUE;
        }
    }

    reply_message = ibus_message_new_error_printf (message,
                                DBUS_ERROR_UNKNOWN_METHOD,
                                "Can not find %s method",
                                ibus_message_get_member (message));

    ibus_connection_send (IBUS_CONNECTION (connection), reply_message);
    ibus_message_unref (reply_message);
    return FALSE;
}


gboolean
bus_input_context_is_focus (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    return priv->has_focus;
}

void
bus_input_context_focus_in (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    if (priv->has_focus)
        return;

    priv->has_focus = TRUE;

    if (priv->engine) {
        bus_engine_proxy_focus_in (priv->engine);
    }

    if (priv->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context, context_signals[FOCUS_IN], 0);
    }
}

void
bus_input_context_focus_out (BusInputContext *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    if (!priv->has_focus)
        return;
    
    priv->has_focus = FALSE;
    
    if (priv->engine) {
        bus_engine_proxy_focus_out (priv->engine);
    }

    if (priv->capabilities & IBUS_CAP_FOCUS) {
        g_signal_emit (context, context_signals[FOCUS_OUT], 0);
    }
}

static void
_engine_destroy_cb (BusEngineProxy      *engine,
                    BusInputContext     *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);
    
    g_assert (engine == priv->engine);

    priv->enabled = FALSE;

    g_object_unref (priv->engine);
    priv->engine = NULL;
}

static void
_factory_destroy_cb (BusFactoryProxy    *factory,
                     BusInputContext    *context)
{
    g_assert (BUS_IS_FACTORY_PROXY (factory));
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (factory == priv->factory);

    priv->enabled = FALSE;

    g_object_unref (priv->factory);
    priv->factory = NULL;

    if (priv->engine != NULL) {
        ibus_object_destroy (IBUS_OBJECT (priv->engine));
        priv->engine = NULL;
    }
}

static void
_engine_commit_string_cb (BusEngineProxy    *engine,
                          const gchar       *text,
                          BusInputContext   *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (text != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                 ibus_service_get_path  (IBUS_SERVICE (context)),
                                 IBUS_INTERFACE_INPUT_CONTEXT,
                                 "CommitString",
                                 G_TYPE_STRING, &text,
                                 G_TYPE_INVALID);
                                 
}

static void
_engine_forward_key_event_cb (BusEngineProxy    *engine,
                              guint              keyval,
                              gboolean           is_press,
                              guint              state,
                              BusInputContext   *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                 ibus_service_get_path (IBUS_SERVICE (context)),
                                 IBUS_INTERFACE_INPUT_CONTEXT,
                                 "ForwardKeyEvent",
                                 G_TYPE_UINT,  &keyval,
                                 G_TYPE_BOOLEAN, &is_press,
                                 G_TYPE_UINT,  &state,
                                 G_TYPE_INVALID);
                                 
}

static void
_engine_update_preedit_cb (BusEngineProxy   *engine,
                           const gchar      *text,
                           IBusAttrList     *attr_list,
                           gint              cursor_pos,
                           gboolean          visible,
                           BusInputContext  *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (text != NULL);
    g_assert (attr_list != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    if (priv->capabilities & IBUS_CAP_PREEDIT) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "UpdatePreedit",
                                     G_TYPE_STRING, &text,
                                     IBUS_TYPE_ATTR_LIST, &attr_list,
                                     G_TYPE_INT, &cursor_pos,
                                     G_TYPE_BOOLEAN, &visible,
                                     G_TYPE_INVALID);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_PREEDIT],
                       0,
                       text,
                       attr_list,
                       cursor_pos,
                       visible);
    }
                                 
}

static void
_engine_update_aux_string_cb (BusEngineProxy   *engine,
                              const gchar      *text,
                              IBusAttrList     *attr_list,
                              gboolean          visible,
                              BusInputContext  *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (text != NULL);
    g_assert (attr_list != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    if (priv->capabilities & IBUS_CAP_AUX_STRING) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "UpdateAuxString",
                                     G_TYPE_STRING, &text,
                                     IBUS_TYPE_ATTR_LIST, &attr_list,
                                     G_TYPE_BOOLEAN, &visible,
                                     G_TYPE_INVALID);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_AUX_STRING],
                       0,
                       text,
                       attr_list,
                       visible);
    }
}

static void
_engine_update_lookup_table_cb (BusEngineProxy   *engine,
                                IBusLookupTable  *table,
                                gboolean          visible,
                                BusInputContext  *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (table != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    if (priv->capabilities & IBUS_CAP_LOOKUP_TABLE) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "UpdateLookupTable",
                                     IBUS_TYPE_LOOKUP_TABLE, &table,
                                     G_TYPE_BOOLEAN, &visible,
                                     G_TYPE_INVALID);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_LOOKUP_TABLE],
                       0,
                       table,
                       visible);
    }                            
}

static void
_engine_register_properties_cb (BusEngineProxy  *engine,
                                IBusPropList    *prop_list,
                                BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (prop_list != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    if (priv->capabilities & IBUS_CAP_PROPERTY) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "RegisterProperties",
                                     IBUS_TYPE_PROP_LIST, &prop_list,
                                     G_TYPE_INVALID);
    }
    else {
        g_signal_emit (context,
                       context_signals[REGISTER_PROPERTIES],
                       0,
                       prop_list);
    }                            
}

static void
_engine_update_property_cb (BusEngineProxy  *engine,
                            IBusProperty    *prop,
                            BusInputContext *context)
{
    g_assert (BUS_IS_ENGINE_PROXY (engine));
    g_assert (prop != NULL);
    g_assert (BUS_IS_INPUT_CONTEXT (context));
    
    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    g_assert (priv->engine == engine);
    g_assert (priv->connection != NULL);

    if (priv->capabilities & IBUS_CAP_PROPERTY) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "UpdateProperty",
                                     IBUS_TYPE_PROPERTY, &prop,
                                     G_TYPE_INVALID);
    }
    else {
        g_signal_emit (context,
                       context_signals[UPDATE_PROPERTY],
                       0,
                       prop);
    }
}

#define DEFINE_FUNCTION(name, Name, signal_name, cap)           \
    static void                                                 \
    _engine_##name##_cb (BusEngineProxy   *engine,              \
                         BusInputContext *context)              \
    {                                                           \
        g_assert (BUS_IS_ENGINE_PROXY (engine));                \
        g_assert (BUS_IS_INPUT_CONTEXT (context));              \
                                                                \
        BusInputContextPrivate *priv;                           \
        priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);         \
                                                                \
        g_assert (priv->engine == engine);                      \
        g_assert (priv->connection != NULL);                    \
                                                                \
        if ((priv->capabilities & (cap)) == (cap)) {            \
            ibus_connection_send_signal (                       \
                IBUS_CONNECTION (priv->connection),             \
                ibus_service_get_path (IBUS_SERVICE (context)), \
                IBUS_INTERFACE_INPUT_CONTEXT,                   \
                #Name,                                          \
                G_TYPE_INVALID);                                \
        }                                                       \
        else {                                                  \
            g_signal_emit (context,                             \
                       context_signals[signal_name],            \
                       0);                                      \
        }                                                       \
}

DEFINE_FUNCTION (show_preedit, ShowPreedit, SHOW_PREEDIT, IBUS_CAP_PREEDIT)
DEFINE_FUNCTION (hide_preedit, HidePreedit, HIDE_PREEDIT, IBUS_CAP_PREEDIT)
DEFINE_FUNCTION (show_aux_string, ShowAuxString, SHOW_AUX_STRING, IBUS_CAP_AUX_STRING)
DEFINE_FUNCTION (hide_aux_string, HideAuxString, HIDE_AUX_STRING, IBUS_CAP_AUX_STRING)
DEFINE_FUNCTION (show_lookup_table, ShowLookupTable, SHOW_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)
DEFINE_FUNCTION (hide_lookup_table, HideLookupTable, HIDE_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)
DEFINE_FUNCTION (page_up_lookup_table, PageUpLookupTable, PAGE_UP_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)
DEFINE_FUNCTION (page_down_lookup_table, PageDownLookupTable, PAGE_DOWN_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)
DEFINE_FUNCTION (cursor_up_lookup_table, CursorUpLookupTable, CURSOR_UP_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)
DEFINE_FUNCTION (cursor_down_lookup_table, CursorDownLookupTable, CURSOR_DOWN_LOOKUP_TABLE, IBUS_CAP_LOOKUP_TABLE)

#undef DEFINE_FUNCTION

static void
bus_input_context_enable_or_disabe_engine (BusInputContext    *context)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    BusInputContextPrivate *priv;
    priv = BUS_INPUT_CONTEXT_GET_PRIVATE (context);

    if (priv->factory == NULL) {
        priv->factory = bus_ibus_impl_get_default_factory (BUS_DEFAULT_IBUS);

        if (priv->factory) {
            g_signal_connect (priv->factory,
                              "destroy",
                              G_CALLBACK (_factory_destroy_cb),
                              context);
            g_object_ref (priv->factory);

            priv->engine = bus_factory_create_engine (priv->factory);

            gint i;
            const static struct {
                const gchar *name;
                GCallback    callback;
            } signals [] = {
                { "commit-string",      G_CALLBACK (_engine_commit_string_cb) },
                { "forward-key-event",  G_CALLBACK (_engine_forward_key_event_cb) },
                { "update-preedit",     G_CALLBACK (_engine_update_preedit_cb) },
                { "show-preedit",       G_CALLBACK (_engine_show_preedit_cb) },
                { "hide-preedit",       G_CALLBACK (_engine_hide_preedit_cb) },
                { "update-aux-string",  G_CALLBACK (_engine_update_aux_string_cb) },
                { "show-aux-string",    G_CALLBACK (_engine_show_aux_string_cb) },
                { "hide-aux-string",    G_CALLBACK (_engine_hide_aux_string_cb) },
                { "update-lookup-table",G_CALLBACK (_engine_update_lookup_table_cb) },
                { "show-lookup-table",  G_CALLBACK (_engine_show_lookup_table_cb) },
                { "hide-lookup-table",  G_CALLBACK (_engine_hide_lookup_table_cb) },
                { "page-up-lookup-table",       G_CALLBACK (_engine_page_up_lookup_table_cb) },
                { "page-down-lookup-table",     G_CALLBACK (_engine_page_down_lookup_table_cb) },
                { "cursor-up-lookup-table",     G_CALLBACK (_engine_cursor_up_lookup_table_cb) },
                { "cursor-down-lookup-table",   G_CALLBACK (_engine_cursor_down_lookup_table_cb) },
                { "register-properties",        G_CALLBACK (_engine_register_properties_cb) },
                { "update-property",            G_CALLBACK (_engine_update_property_cb) },
                { NULL, 0 }
            };

            for (i = 0; signals[i].name != NULL; i++) {
                g_signal_connect (priv->engine,
                                  signals[i].name,
                                  signals[i].callback,
                                  context);
            }

        }
    }

    if (priv->factory == NULL || priv->engine == NULL)
        return;

    priv->enabled = ! priv->enabled;

    if (priv->enabled) {
        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "Enabled",
                                     G_TYPE_INVALID);


        bus_engine_proxy_enable (priv->engine);
    }
    else {
        bus_engine_proxy_disable (priv->engine);

        ibus_connection_send_signal (IBUS_CONNECTION (priv->connection),
                                     ibus_service_get_path  (IBUS_SERVICE (context)),
                                     IBUS_INTERFACE_INPUT_CONTEXT,
                                     "Disabled",
                                     G_TYPE_INVALID);
    }
}

static gboolean
bus_input_context_filter_keyboard_shortcuts (BusInputContext    *context,
                                             guint               keyval,
                                             gboolean            is_press,
                                             guint               state)
{
    g_assert (BUS_IS_INPUT_CONTEXT (context));

    state &= IBUS_MODIFIER_MASK;

    if (is_press && keyval == IBUS_space && (state & IBUS_CONTROL_MASK)) {
        bus_input_context_enable_or_disabe_engine (context);
        return TRUE;
    }
    return FALSE;
}
