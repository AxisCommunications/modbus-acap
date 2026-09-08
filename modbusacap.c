/**
 * Copyright (C) 2023, Axis Communications AB, Lund, Sweden
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>
#include <axevent.h>
#include <axparameter.h>
#include <errno.h>
#include <libgen.h>
#include <pthread.h>

#include "modbus_client.h"
#include "modbus_server.h"
#include "modbusacap_common.h"

enum Mode
{
    SERVER = 0,
    CLIENT = 1
};

static GMainLoop *main_loop_ = NULL;
static AXEventHandler *ehandler_;
static AXParameter *axparameter_ = NULL;
static gboolean initialized_ = FALSE;
static guint16 address_ = 0;
static guint8 mode_ = 0;
static guint32 port_ = 0;
static gchar *server_ = NULL;
static guint subscription_base_;
static guint subscription_threshold_;
static pthread_mutex_t lock_ = PTHREAD_MUTEX_INITIALIZER;

static void open_syslog(const char *app_name)
{
    openlog(app_name, LOG_PID, LOG_LOCAL4);
}

static void close_syslog(void)
{
    LOG_I("✅ Exiting!");
    closelog();
}

static void event_callback(guint subscription, AXEvent *event, void *data)
{
    const AXEventKeyValueSet *key_value_set;
    gboolean active;

    (void)subscription;
    (void)data;

    // Handle event
    key_value_set = ax_event_get_key_value_set(event);

    if (ax_event_key_value_set_get_boolean(key_value_set, "active", NULL, &active, NULL))
    {
        pthread_mutex_lock(&lock_);
        LOG_I(
            "ⓘ aoa-event %s active (%s)",
            active ? "is" : "NOT",
            CLIENT == mode_ ? "running in client mode, passing on via Modbus"
                            : "running in server mode, not forwarded anywhere");
        // Send event over Modbus
        if (CLIENT == mode_)
        {
            if (!modbus_client_send_event(address_, active))
            {
                LOG_E("%s/%s: Failed to send event data over Modbus", __FILE__, __FUNCTION__);
            }
        }
        pthread_mutex_unlock(&lock_);
    }
    else
    {
        LOG_I("ⓘ Received event without boolean value 'active' (probably not a stateful event)");
    }

    // Free the received event, n.b. AXEventKeyValueSet should not be freed
    // since it's owned by the event system until unsubscribing
    ax_event_free(event);
}

static guint aoatrigger_subscription(const guint newscenario, const gchar *subtype)
{
    assert(NULL != ehandler_);

    AXEventKeyValueSet *key_value_set;
    guint subscription;

    key_value_set = ax_event_key_value_set_new();

    // Setup subscription string and key/value set for subscription
    gchar *subscriptionstr = g_strdup_printf("Device1Scenario%u%s", newscenario, NULL == subtype ? "" : subtype);
    LOG_I("⏳ Creating subscription for '%s' ...", subscriptionstr);
    ax_event_key_value_set_add_key_values(
        key_value_set,
        NULL,
        "topic0",
        "tnsaxis",
        "CameraApplicationPlatform",
        AX_VALUE_TYPE_STRING,
        "topic1",
        "tnsaxis",
        "ObjectAnalytics",
        AX_VALUE_TYPE_STRING,
        "topic2",
        "tnsaxis",
        subscriptionstr,
        AX_VALUE_TYPE_STRING,
        NULL);

    // Setup subscription and connect to callback function
    ax_event_handler_subscribe(
        ehandler_,                              // event handler
        key_value_set,                          // key value set
        &subscription,                          // subscription id
        (AXSubscriptionCallback)event_callback, // callback function
        NULL,                                   // user data
        NULL);                                  // GError

    // Cleanup and return subscription id
    ax_event_key_value_set_free(key_value_set);
    LOG_I("✅ AOA trigger subscription id for %s is %d", subscriptionstr, subscription);
    g_free(subscriptionstr);
    return subscription;
}

static void teardown_event_subscriptions(void)
{
    assert(NULL != ehandler_);

    (void)ax_event_handler_unsubscribe(ehandler_, subscription_base_, NULL);
    (void)ax_event_handler_unsubscribe(ehandler_, subscription_threshold_, NULL);
}

static void setup_event_subscriptions(const guint newscenario)
{
    assert(NULL != ehandler_);

    // Unsubscribe from eventual existing subscriptions
    teardown_event_subscriptions();

    // New subscriptions; we currently subscribe to and handle the stateful (active/inactive)
    // events with topic2 set to (and X = 1, 2 ... N):
    // - "Device1ScenarioX"
    // - "Device1ScenarioXThreshold"
    subscription_base_ = aoatrigger_subscription(newscenario, NULL);
    subscription_threshold_ = aoatrigger_subscription(newscenario, "Threshold");
}

static gboolean setup_modbus(const guint8 mode, const guint32 port, const gchar *server)
{
    switch (mode)
    {
    case SERVER:
        return modbus_server_start(port);
    case CLIENT:
        assert(NULL != server);
        return modbus_client_init(server, port);
    default:
        LOG_E("%s/%s: %u is not a known mode", __FILE__, __FUNCTION__, mode);
        break;
    }
    return FALSE;
}

static gchar *get_param(AXParameter *axparameter, const gchar *name)
{
    assert(NULL != axparameter);
    GError *error = NULL;
    gchar *value = NULL;
    if (!ax_parameter_get(axparameter, name, &value, &error))
    {
        LOG_E("%s/%s: failed to get %s parameter", __FILE__, __FUNCTION__, name);
        if (NULL != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return NULL;
    }
    LOG_I("ⓘ Got %s value '%s'", name, value);
    return value;
}

static void close_current_modbus(const guint8 m)
{
    switch (m)
    {
    case SERVER:
        modbus_server_stop();
        break;
    case CLIENT:
        modbus_client_cleanup();
        break;
    default:
        LOG_E("%s/%s: %u is not a known mode", __FILE__, __FUNCTION__, mode_);
        break;
    }
}

static void address_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    const int newaddress = atoi(value);
    assert(0 <= newaddress && G_MAXUINT16 >= newaddress);
    address_ = newaddress;
    LOG_I("✅ %s is now set to %u", name, address_);
}

static void mode_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    const guint8 newmode = atoi(value);
    pthread_mutex_lock(&lock_);
    if (mode_ == newmode)
    {
        pthread_mutex_unlock(&lock_);
        return;
    }
    close_current_modbus(mode_);
    mode_ = newmode;
    assert(SERVER == mode_ || CLIENT == mode_);
    LOG_I("✅ %s is now set to %s'", name, mode_ == SERVER ? "server" : "client");

    // Setup Modbus for this mode
    if (initialized_)
    {
        if (!setup_modbus(mode_, port_, server_))
        {
            LOG_E("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
            assert(FALSE);
        }
    }
    pthread_mutex_unlock(&lock_);
}

static void port_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    const guint32 newport = atoi(value);
    pthread_mutex_lock(&lock_);
    if (port_ == newport)
    {
        pthread_mutex_unlock(&lock_);
        return;
    }
    close_current_modbus(mode_);
    port_ = newport;
    assert(1024 <= port_ && 65535 >= port_);
    LOG_I("✅ %s is now set to %u", name, port_);

    // Setup Modbus for this port
    if (initialized_)
    {
        if (!setup_modbus(mode_, port_, server_))
        {
            LOG_E("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
            assert(FALSE);
        }
    }
    pthread_mutex_unlock(&lock_);
}

static void scenario_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    const guint scenario = atoi(value);
    assert(0 < scenario);
    LOG_I("✅ %s is now set to %u", name, scenario);

    // Update subscription
    setup_event_subscriptions(scenario);
}

static void server_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    pthread_mutex_lock(&lock_);
    if (0 == g_strcmp0(server_, value))
    {
        pthread_mutex_unlock(&lock_);
        return;
    }
    close_current_modbus(mode_);
    g_free(server_);
    server_ = g_strdup(value);
    LOG_I("✅ %s is now set to '%s'", name, value);

    // Setup Modbus for this mode
    if (initialized_ && !setup_modbus(mode_, port_, server_))
    {
        LOG_E("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
    }
    pthread_mutex_unlock(&lock_);
}

static gboolean setup_param(const gchar *name, AXParameterCallback callbackfn)
{
    GError *error = NULL;
    gchar *value = NULL;

    assert(NULL != name);
    assert(NULL != axparameter_);
    assert(NULL != callbackfn);

    if (!ax_parameter_register_callback(axparameter_, name, callbackfn, NULL, &error))
    {
        LOG_E("%s/%s: failed to register %s callback", __FILE__, __FUNCTION__, name);
        if (NULL != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return FALSE;
    }
    value = get_param(axparameter_, name);
    if (NULL == value)
    {
        return FALSE;
    }
    LOG_I("ⓘ Got %s value '%s'", name, value);
    callbackfn(name, value, NULL);
    g_free(value);

    return TRUE;
}

static void signal_handler(gint signal_num)
{
    LOG_I("🛑 %s", strsignal(signal_num));
    switch (signal_num)
    {
    case SIGTERM:
    case SIGABRT:
    case SIGINT:
        LOG_I("🧹 Unsubscribe from events ...");
        teardown_event_subscriptions();
        g_main_loop_quit(main_loop_);
        break;
    default:
        break;
    }
}

static gboolean signal_handler_init(void)
{
    struct sigaction sa = {0};

    if (-1 == sigemptyset(&sa.sa_mask))
    {
        LOG_E("%s/%s: Failed to initialize signal handler: %s", __FILE__, __FUNCTION__, strerror(errno));
        return FALSE;
    }

    sa.sa_handler = signal_handler;

    if (0 > sigaction(SIGTERM, &sa, NULL) || 0 > sigaction(SIGABRT, &sa, NULL) || 0 > sigaction(SIGINT, &sa, NULL))
    {
        LOG_E("%s/%s: Failed to install signal handler: %s", __FILE__, __FUNCTION__, strerror(errno));
        return FALSE;
    }

    return TRUE;
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    const char *app_name = basename(argv[0]);
    open_syslog(app_name);

    int ret = EXIT_SUCCESS;
    if (!signal_handler_init())
    {
        ret = EXIT_FAILURE;
        goto exit_syslog;
    }

    // Create event handler
    ehandler_ = ax_event_handler_new();

    // ACAP parameter setup
    axparameter_ = ax_parameter_new(app_name, &error);
    if (NULL != error)
    {
        LOG_E("%s/%s: ax_parameter_new failed (%s)", __FILE__, __FUNCTION__, error->message);
        g_error_free(error);
        ret = EXIT_FAILURE;
        goto exit_ehandler;
    }
    // clang-format off
    if (!setup_param("ModbusAddress", address_callback) ||
        !setup_param("Mode", mode_callback) ||
        !setup_param("Port", port_callback) ||
        !setup_param("Scenario", scenario_callback) ||
        !setup_param("Server", server_callback))
    // clang-format on
    {
        ret = EXIT_FAILURE;
        goto exit_param;
    }

    // We are initialized, start Modbus handling with the configured values
    initialized_ = TRUE;
    if (!setup_modbus(mode_, port_, server_))
    {
        LOG_E("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
        ret = EXIT_FAILURE;
        goto exit_param;
    }

    // Main loop
    LOG_I("✅ Ready");
    main_loop_ = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(main_loop_);

    // Cleanup and controlled shutdown
    LOG_I("🧹 Unreference main loop ...");
    g_main_loop_unref(main_loop_);
exit_param:
    LOG_I("🧹 Free parameter handler ...");
    ax_parameter_free(axparameter_);
exit_ehandler:
    LOG_I("🧹 Free event handler ...");
    ax_event_handler_free(ehandler_);

    // Cleanup Modbus
    modbus_client_cleanup();
    modbus_server_stop();
    g_free(server_);
exit_syslog:
    LOG_I("🧹 Closing syslog ...");
    close_syslog();

    return ret;
}
