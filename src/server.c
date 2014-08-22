/**
 * Copyright (c) 2014, César Rodas <crodas@php.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Notifd nor the names of its contributors may be used
 *     to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "notifd.h"
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pair.h>

static int psock;
static int epsock;
static uv_timer_t ptimer;

static int _check_pubsubd(uv_timer_t * handle)
{
    do {
        char * buf = NULL;
        int bytes;

        if ((bytes = nn_recv(psock, &buf, NN_MSG, NN_DONTWAIT)) == -1) {
            break;
        }

        json_t * json, * target, *message;
        json_error_t error;
        json = json_loadb(buf, bytes, 0, &error);


        if (json) {
            target = json_object_get(json, "target");
            message = json_object_get(json, "message");
            if (target && message) {
                pubsub_publish((char *)json_string_value(target), message);
            }
            json_decref(json);
        }

        nn_freemsg (buf);
    } while(1);
}


int server_init(Config_t * config)
{
    char tmp[200];

    psock = nn_socket (AF_SP, NN_REP);
    //nn_setsockopt(psock, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val));
    strcpy(tmp, "tcp://*:2508");
    if ((epsock=nn_bind(psock, tmp)) == -1) {
        PANIC(("Failed to listen public pubsub server (%s - %s)", tmp, nn_strerror(errno)));
    }

    uv_timer_init(uv_default_loop(), &ptimer);
    uv_timer_start(&ptimer, (uv_timer_cb) &_check_pubsubd, 1, 1);
}

int server_destroy(Config_t * config)
{
    nn_shutdown(psock, epsock);
    nn_close(psock);
}
