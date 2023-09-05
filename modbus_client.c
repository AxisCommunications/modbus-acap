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
#include <errno.h>
#include <modbus.h>

#include "modbus_client.h"
#include "modbusacap_common.h"

static modbus_t *ctx = NULL;

gboolean modbus_client_send_event(const gboolean active)
{
    assert(NULL != ctx);

    if (1 != modbus_write_bit(ctx, EVENT_ACTIVE_ADDRESS, active))
    {
        LOG_E("%s/%s: Failed to write Modbus (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
    }
    LOG_I("%s/%s: Successfully called modbus_write_bit", __FILE__, __FUNCTION__);
    return TRUE;
}

gboolean modbus_client_init(const gchar *server, const guint32 port)
{
    assert(NULL != server);
    assert(1024 <= port && 65535 >= port);
    modbus_free(ctx);
    LOG_I("Trying to create Modbus TCP context for %s:%u", server, port);
    ctx = modbus_new_tcp(server, port);
    if (NULL == ctx)
    {
        LOG_E("%s/%s: Unable to create the libmodbus context (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        return FALSE;
    }
    if (0 != modbus_connect(ctx))
    {
        LOG_E("%s/%s: Failed to connect (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        modbus_free(ctx);
        return FALSE;
    }
    return TRUE;
}

void modbus_client_cleanup()
{
    modbus_free(ctx);
}
