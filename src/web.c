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

    if (config->web_timeout > 0) {
        /* subscribe for pubsub */
        conn->timeout = (uv_timer_t*)malloc(sizeof(uv_timer_t));
        uv_timer_init(uv_default_loop(), conn->timeout);
        conn->timeout->data = (void *) conn;
        uv_timer_start(conn->timeout, (uv_timer_cb) &conn_listen_no_messages, config->web_timeout, 0);
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
    char * channel = conn->request->url + 9;
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
        json_object_set_new(info, "uptime", json_integer((uint64)(now() - config->start_time)));
        json_object_set_new(info, "requests", json_integer(config->requests));
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
