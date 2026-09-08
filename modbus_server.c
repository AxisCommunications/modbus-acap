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

static gint run_server_ = FALSE;
static pthread_t modbus_server_thread_id_;
static gboolean modbus_server_thread_running_ = FALSE;
static guint32 modbus_port_ = 0;

enum ClientState
{
    CLIENT_DISCONNECTED,
    CLIENT_WAITING,
    CLIENT_CONNECTED
};

static void *run_modbus_server(void *run)
{
    assert(NULL != run);
    modbus_mapping_t *mb_mapping = NULL;
    modbus_t *srv_ctx = NULL;
    int s = -1;

    assert(1024 <= modbus_port_ && 65535 >= modbus_port_);
    LOG_I("⏳ Trying to create Modbus TCP context for all IP addresss and port %u ...", modbus_port_);
    srv_ctx = modbus_new_tcp(NULL, modbus_port_);
    if (NULL == srv_ctx)
    {
        LOG_E("%s/%s: Unable to create the libmodbus context (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }
    if (0 != modbus_set_indication_timeout(srv_ctx, 1, 0))
    {
        LOG_E("%s/%s: Failed to set Modbus indication timeout (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    LOG_I("⏳ Start listening for Modbus TCP connection ...");
    s = modbus_tcp_listen(srv_ctx, 1);
    if (-1 == s)
    {
        LOG_E("%s/%s: modbus_tcp_listen failed (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    const int flags = fcntl(s, F_GETFL, 0);
    if (-1 == fcntl(s, F_SETFL, flags | O_NONBLOCK))
    {
        LOG_E("%s/%s: fcntl failed for socket (%s)", __FILE__, __FUNCTION__, strerror(errno));
        goto server_exit;
    }

    LOG_I("⏳ Allocate mapping ...");
    mb_mapping = modbus_mapping_new(65536, 0, 0, 0);
    if (NULL == mb_mapping)
    {
        LOG_E("%s/%s: Failed to allocate the mapping: %s", __FILE__, __FUNCTION__, modbus_strerror(errno));
        goto server_exit;
    }

    uint8_t req[MODBUS_TCP_MAX_ADU_LENGTH];
    enum ClientState client_state = CLIENT_DISCONNECTED;
    while (g_atomic_int_get((gint *)run))
    {
        if (CLIENT_DISCONNECTED == client_state)
        {
            LOG_I("⏳ Waiting for Modbus TCP connection on port %u ...", modbus_port_);
            client_state = CLIENT_WAITING;
        }
        if (CLIENT_WAITING == client_state)
        {
            // Attempt to accept a client connection (non-blocking)
            if (0 >= modbus_tcp_accept(srv_ctx, &s))
            {
                // Sleep briefly to avoid busy-waiting
                usleep(200000); // Sleep for 200 ms
                continue;
            }
            client_state = CLIENT_CONNECTED;
            LOG_I("⏳ Client connected, start receiving ...");
        }

        int rlen = modbus_receive(srv_ctx, req);
        if (-1 == rlen)
        {
            if (ETIMEDOUT == errno)
            {
                // Timeout is expected, continue to wait for requests
                continue;
            }

            LOG_I("ⓘ Modbus client disconnected (%s)", modbus_strerror(errno));
            modbus_close(srv_ctx);
            client_state = CLIENT_DISCONNECTED;
            continue;
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
            LOG_E(
                "%s/%s: The requests we handle should be longer than the %d bytes we now got",
                __FILE__,
                __FUNCTION__,
                rlen);
            modbus_close(srv_ctx);
            client_state = CLIENT_DISCONNECTED;
            continue;
        }
        guint16 address = (req[8] << 8) | req[9];
        LOG_I("ⓘ Received request on address %d", address);
        if (MODBUS_FC_WRITE_SINGLE_COIL == req[7])
        {
            LOG_I("ⓘ The event trigger on the remote device is now %s", 0xFF == req[10] ? "ACTIVE" : "INACTIVE");
            if (-1 == modbus_reply(srv_ctx, req, rlen, mb_mapping))
            {
                LOG_E("%s/%s: modbus_reply failed (%s)", __FILE__, __FUNCTION__, modbus_strerror(errno));
                modbus_close(srv_ctx);
                client_state = CLIENT_DISCONNECTED;
                continue;
            }
            LOG_I("✅ Reply sent to client for acknowledgement");
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
    g_atomic_int_set(&run_server_, TRUE);
    modbus_port_ = port;
    int result = pthread_create(&modbus_server_thread_id_, NULL, run_modbus_server, &run_server_);
    if (0 != result)
    {
        LOG_E("%s/%s: Failed to create thread (%s)", __FILE__, __FUNCTION__, strerror(result));
        return FALSE;
    }
    modbus_server_thread_running_ = TRUE;
    return TRUE;
}

void modbus_server_stop()
{
    g_atomic_int_set(&run_server_, FALSE);
    if (modbus_server_thread_running_)
    {
        LOG_I("⏳ Joining running server thread ...");
        pthread_join(modbus_server_thread_id_, NULL);
        modbus_server_thread_running_ = FALSE;
    }
}
