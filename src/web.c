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

Config_t * config;

static void conn_listen_no_messages(uv_timer_t * handle)
{
    FETCH_CONNECTION_UV;

    RESPONSE_SET("messages", json_array());
    RESPONSE_TRUE("timeout");
    RESPONSE_SET("n", json_integer(0));
    http_send_response(conn);
}

static int do_subscribe(http_connection_t * conn)
{
    char * channel = conn->request->url + 9;
    int web_timeout = config->web_timeout;
    char  * timeout = $_GET("timeout");
    int ttimeout = 0;

    if (timeout && (ttimeout=atoi(timeout)) && BETWEEN(ttimeout, 1, 60)) {
        web_timeout = ttimeout * 1000;
    }

    if (web_timeout > 0) {
        /* subscribe for pubsub */
        conn->timeout = (uv_timer_t*)malloc(sizeof(uv_timer_t));
        uv_timer_init(uv_default_loop(), conn->timeout);
        conn->timeout->data = (void *) conn;
        uv_timer_start(conn->timeout, (uv_timer_cb) &conn_listen_no_messages, web_timeout, 0);
        pubsub_subscription_create(channel, conn);
        ADDREF(conn);
        return;
    }

    conn_listen_no_messages(NULL);
}

void _messages_query_result(void * data, json_t * messages)
{
    http_connection_t * conn = (http_connection_t *) data;
    if (messages) { 
        RESPONSE_SET("messages", messages);
        RESPONSE_TRUE("db");
        RESPONSE_FALSE("pubsub");
        RESPONSE_SET("n", json_integer(json_array_size(messages)));
        http_send_response(conn);
    } else {
        do_subscribe(data);
    }
}


static int do_listen(http_connection_t * conn)
{
    char * channel = conn->request->uri + 9;
    database_query(channel, _messages_query_result, (void *) conn);
    RESPONSE_STRING("channel", channel);
}

static int get_version(http_connection_t * conn)
{
    RESPONSE_STRING("version", VERSION);
}

static int get_info(http_connection_t * conn)
{
    json_t * info;

    info = json_object();

    if (!check_ip("127.0.0.1", conn)) {
        RESPONSE_TRUE("error");
        RESPONSE_STRING("error_str", "Permission denied");
    } else {
        json_object_set_new(info, "version", json_string(VERSION));
        json_object_set_new(info, "host", json_string(config->web_ip));
        json_object_set_new(info, "port", json_integer(config->web_port));
        json_object_set_new(info, "uptime", json_real(now() - config->start_time));
        json_object_set_new(info, "requests", json_integer(config->requests));
        printf("%f\n", now() - config->start_time);
    }

    RESPONSE_SET("info", info);

    http_send_response(conn);
}

static int do_ack(http_connection_t * conn) {
}

static int do_ack_id(http_connection_t * conn) {
}

BEGIN_ROUTES(web_routes)
    ROUTE("/channel/*", do_listen)
    ROUTE_EX(HTTP_POST,"/ack/_id/*", do_ack_id)
    ROUTE_EX(HTTP_POST,"/ack/*", do_ack)
    ROUTE_EX(HTTP_POST, "/channel/*", do_listen)
    ROUTE_EX(HTTP_GET, "/version", get_version)
    ROUTE_EX(HTTP_GET, "/info", get_info)
END_ROUTES

http_routes_t * webroutes_get(Config_t * _config)
{
    config = _config;
    return web_routes;
}
