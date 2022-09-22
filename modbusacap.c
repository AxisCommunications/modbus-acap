/**
 * Copyright (C) 2022, Axis Communications AB, Lund, Sweden
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
#include <libgen.h>
#include <param.h>

#include "modbus_client.h"
#include "modbus_server.h"
#include "modbusacap_common.h"

enum Mode
{
    SERVER = 0,
    CLIENT = 1
};

static AXEventHandler *ehandler;
static AXParameter *axparameter = NULL;
static gboolean initialized = FALSE;
static guint8 mode = 0;
static guint subscription;

static void open_syslog(const char *app_name)
{
    openlog(app_name, LOG_PID, LOG_LOCAL4);
}

static void close_syslog(void)
{
    LOG_I("Exiting!");
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
        LOG_I("aoa-event %s active", active ? "is" : "NOT");
        // Send event over Modbus
        if (CLIENT == mode)
        {
            if (!modbus_client_send_event(active))
            {
                LOG_E("%s/%s: Failed to send event data over Modbus", __FILE__, __FUNCTION__);
            }
        }
    }
    else
    {
        LOG_I("%s/%s: Failed to get boolean value active from event", __FILE__, __FUNCTION__);
    }

    // Free the received event, n.b. AXEventKeyValueSet should not be freed
    // since it's owned by the event system until unsubscribing
    ax_event_free(event);
}

static guint aoatrigger_subscription(const guint newscenario)
{
    assert(NULL != ehandler);

    AXEventKeyValueSet *key_value_set;
    guint subscription;

    key_value_set = ax_event_key_value_set_new();

    // Setup subscription string; for a scenario with name "Scenario 1" it will
    // be "Device1Scenario1"
    const gchar *devicestr = "Device1Scenario";
    gchar *subscriptionstr = malloc(strlen(devicestr) + 3);
    sprintf(subscriptionstr, "%s%u", devicestr, newscenario);
    gchar *src;
    gchar *dst;
    for (dst = src = subscriptionstr; '\0' != *src; src++)
    {
        *dst = *src;
        if (' ' != *dst)
        {
            dst++;
        }
    }
    *dst = '\0';

    LOG_I("%s/%s: Create subscription for '%s'", __FILE__, __FUNCTION__, subscriptionstr);
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
        ehandler,                               // event handler
        key_value_set,                          // key value set
        &subscription,                          // subscription id
        (AXSubscriptionCallback)event_callback, // callback function
        NULL,                                   // user data
        NULL);                                  // GError

    // Cleanup
    g_free(subscriptionstr);
    ax_event_key_value_set_free(key_value_set);

    // Return subscription id
    LOG_I("%s/%s: AOA trigger subscription id: %d", __FILE__, __FUNCTION__, subscription);
    return subscription;
}

static void setup_event_subscription(guint *subscription, const guint newscenario)
{
    assert(NULL != ehandler);
    assert(NULL != subscription);

    // Unsubscribe to eventual existing subscription
    (void)ax_event_handler_unsubscribe(ehandler, *subscription, NULL);

    // New subscription
    *subscription = aoatrigger_subscription(newscenario);
}

static gboolean setup_modbus(const guint mode, const gchar *server)
{
    switch (mode)
    {
    case SERVER:
        return modbus_server_start();
    case CLIENT:
        assert(NULL != server);
        return modbus_client_init(server);
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
    LOG_I("Got %s value: %s", name, value);
    return value;
}

static void scenario_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    guint scenario = atoi(value);
    assert(0 < scenario);
    assert(100 > scenario);
    LOG_I("%s/%s: Got new %s (%u)", __FILE__, __FUNCTION__, name, scenario);

    // Update subscription
    setup_event_subscription(&subscription, scenario);
}

static void server_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    LOG_I("%s/%s: Got new %s (%s)", __FILE__, __FUNCTION__, name, value);

    // Setup Modbus for this mode
    if (initialized && !setup_modbus(mode, value))
    {
        LOG_I("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
    }
}

static void mode_callback(const gchar *name, const gchar *value, void *data)
{
    (void)data;
    if (NULL == value)
    {
        LOG_E("%s/%s: Unexpected NULL value for %s", __FILE__, __FUNCTION__, name);
        return;
    }

    mode = atoi(value);
    assert(0 == mode || 1 == mode);
    LOG_I("%s/%s: Got new %s (%s)", __FILE__, __FUNCTION__, name, mode == SERVER ? "server" : "client");

    // Setup Modbus for this mode
    if (initialized && !setup_modbus(mode, get_param(axparameter, "Server")))
    {
        LOG_I("%s/%s: Failed to setup Modbus", __FILE__, __FUNCTION__);
    }
}

static gboolean setup_param(const gchar *name, AXParameterCallback callbackfn)
{
    GError *error = NULL;
    gchar *value = NULL;

    assert(NULL != name);
    assert(NULL != axparameter);
    assert(NULL != callbackfn);

    if (!ax_parameter_register_callback(axparameter, name, callbackfn, NULL, &error))
    {
        LOG_E("%s/%s: failed to register %s callback", __FILE__, __FUNCTION__, name);
        if (NULL != error)
        {
            LOG_E("%s/%s: %s", __FILE__, __FUNCTION__, error->message);
            g_error_free(error);
        }
        return FALSE;
    }
    value = get_param(axparameter, name);
    if (NULL == value)
    {
        return FALSE;
    }
    LOG_I("%s/%s: Got %s value: %s", __FILE__, __FUNCTION__, name, value);
    callbackfn(name, value, NULL);
    g_free(value);

    return TRUE;
}

static void init_signals(void)
{
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGPIPE);
    sa.sa_flags = 0;
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, NULL);
}

int main(int argc, char **argv)
{
    GError *error = NULL;
    int ret = 0;

    init_signals();

    char *app_name = basename(argv[0]);
    open_syslog(app_name);

    // Create event handler
    ehandler = ax_event_handler_new();

    // ACAP parameter setup
    axparameter = ax_parameter_new(app_name, &error);
    if (NULL != error)
    {
        LOG_E("%s/%s: ax_parameter_new failed (%s)", __FILE__, __FUNCTION__, error->message);
        g_error_free(error);
        ret = 1;
        goto exit_ehandler;
    }
    // clang-format off
    if (!setup_param("Mode", mode_callback) ||
        !setup_param("Scenario", scenario_callback) ||
        !setup_param("Server", server_callback))
    // clang-format on
    {
        ret = 1;
        goto exit_param;
    }

    // We are initialized, trigger start of Modbus handling via parameter callback
    initialized = TRUE;
    mode_callback("Mode", get_param(axparameter, "Mode"), NULL);

    // Main loop
    LOG_I("%s/%s: Ready", __FILE__, __FUNCTION__);
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    // Cleanup and controlled shutdown
exit_param:
    LOG_I("%s/%s: Free parameter handler ...", __FILE__, __FUNCTION__);
    ax_parameter_free(axparameter);
exit_ehandler:
    LOG_I("%s/%s: Unsubscribe from events ...", __FILE__, __FUNCTION__);
    ax_event_handler_unsubscribe(ehandler, subscription, NULL);
    ax_event_handler_free(ehandler);

    // Cleanup Modbus
    modbus_client_cleanup();
    modbus_server_stop();

    LOG_I("%s/%s: Closing syslog ...", __FILE__, __FUNCTION__);
    close_syslog();

    return ret;
}
