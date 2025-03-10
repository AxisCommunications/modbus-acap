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
#include <fcntl.h>
#include <modbus.h>
#include <pthread.h>
#include <unistd.h>

#include "modbus_server.h"
#include "modbusacap_common.h"

static gboolean run_server = FALSE;
static pthread_t modbus_server_thread_id = -1;
static guint32 modbus_port = 0;
static modbus_t *srv_ctx = NULL;

static void *run_modbus_server(void *run)
{
    assert(NULL != run);
    modbus_mapping_t *mb_mapping = NULL;
    int s;
    int flags;

    assert(1024 <= modbus_port && 65535 >= modbus_port);
    LOG_I("Trying to create Modbus TCP context for all IP addresss and port %u ...", modbus_port);
    srv_ctx = modbus_new_tcp(NULL, modbus_port);
    if (NULL == srv_ctx)
    {
        LOG_E("%s/%s: Unable to create the libmodbus context (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    LOG_I("Listen for Modbus TCP connection ...");
    s = modbus_tcp_listen(srv_ctx, 1);
    if (-1 == s)
    {
        LOG_E("%s/%s: modbus_tcp_listen failed (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    flags = fcntl(s, F_GETFL, 0);
    if (-1 == fcntl(s, F_SETFL, flags | O_NONBLOCK))
    {
        LOG_E("%s/%s: fcntl failed for socket (%s)", __FILE__, __FUNCTION__, strerror(errno));
        goto server_exit;
    }

    LOG_I("Accept Modbus TCP connection ...");
    while (*((gboolean *)run))
    {
        // Attempt to accept a client connection (non-blocking)
        if (0 < modbus_tcp_accept(srv_ctx, &s))
        {
            break;
        }

        // Sleep briefly to avoid busy-waiting
        usleep(200000); // Sleep for 200 ms
    }

    LOG_I("Allocate mapping ...");
    mb_mapping = modbus_mapping_new(1, 0, 0, 0);
    if (NULL == mb_mapping)
    {
        LOG_E("%s/%s: Failed to allocate the mapping: %s", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    LOG_I("%s/%s: Start receiving ...", __FILE__, __FUNCTION__);
    uint8_t req[MODBUS_TCP_MAX_ADU_LENGTH];
    while (*((gboolean *)run))
    {
        int rlen = modbus_receive(srv_ctx, req);
        if (-1 == rlen)
        {
            LOG_E("%s/%s: modbus_receive failed (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
            break;
        }
#if 0
    LOG_I(
      "%s/%s: We received (length %d): [ %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ]",
      __FILE__,
      __FUNCTION__,
      rlen,
      req[0],
      req[1],
      req[2],
      req[3],
      req[4],
      req[5],
      req[6],
      req[7],
      req[8],
      req[9],
      req[10],
      req[11]);
#endif
        if (12 > rlen)
        {
            LOG_I(
                "%s/%s: The requests we handle should be longer than the %d bytes we now got",
                __FILE__,
                __FUNCTION__,
                rlen);
            break;
        }
        guint16 address = (req[8] << 8) | req[9];
        LOG_I("%s/%s: Received request on address %d", __FILE__, __FUNCTION__, address);
        if (MODBUS_FC_WRITE_SINGLE_COIL == req[7])
        {
            LOG_I(
                "%s/%s: The event trigger on the remote device is now %s",
                __FILE__,
                __FUNCTION__,
                0xFF == req[10] ? "ACTIVE" : "INACTIVE");
            if (-1 == modbus_reply(srv_ctx, req, rlen, mb_mapping))
            {
                LOG_E("%s/%s: modbus_reply failed (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
                break;
            }
            LOG_I("%s/%s: Send reply to client for acknowledgement", __FILE__, __FUNCTION__);
        }
    }

server_exit:
    if (0 < s)
    {
        close(s);
    }
    modbus_mapping_free(mb_mapping);
    modbus_free(srv_ctx);

    pthread_exit(NULL);
}

gboolean modbus_server_start(const guint32 port)
{
    modbus_server_stop();
    run_server = TRUE;
    modbus_port = port;
    int result = pthread_create(&modbus_server_thread_id, NULL, run_modbus_server, &run_server);
    if (0 != result)
    {
        LOG_E("%s/%s: Failed to create thread (%s)", __FILE__, __FUNCTION__, strerror(result));
        return FALSE;
    }
    return TRUE;
}

void modbus_server_stop()
{
    run_server = FALSE;
    if (0 < (int)modbus_server_thread_id)
    {
        LOG_I("%s/%s: Joining running server thread ...", __FILE__, __FUNCTION__);
        pthread_join(modbus_server_thread_id, NULL);
    }
    modbus_server_thread_id = -1;
    srv_ctx = NULL;
}
