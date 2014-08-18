/* config.h -- 
 *
 * Copyright (C) <year> <copyright holder>
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license.  See the LICENSE file for details.
 */

#ifndef NOTIFD_H
#define NOTIFD_H
#include <uv.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <ctype.h>
#include <http_parser.h>
#include "dict.h"
#include <jansson.h>

typedef long long uint64;

#define VERSION         "0.0.1-beta"

struct Config {
    /* Web settings {{{ */
    char        * web_ip;
    int         web_port;
    char        * web_allow_origin;
    uv_tcp_t    web_server;
    int         web_timeout;
    uint64      requests;      
    /*  How many un-delivered notifications
     *  shoul be keep in memory?
     */
    int         web_history;
    struct sockaddr_in    web_bind;
    /* }}} */

    uv_loop_t   * uv_loop;

    double      start_time;
    short       debug;
    char        *db_path;
};

struct http_body {
    char * buffer;
    size_t len;
};

struct http_response
{
    int status;
    int http_major;
    int http_minor;

    dict * headers;
    char * ptr;

    char header_ptr[4096];
    size_t header_len;

    short debug;

    json_t * object;
};

struct http_request {
    int method;
    dict * headers;
    char * url;

    struct http_body body;

    int http_major;
    int http_minor;
};

struct http_connection
{
    uv_tcp_t        stream;
    http_parser     parser;
    uv_write_t      write_req;

    double time;

    struct http_request * request;
    struct http_response * response;

    int last_was_value;
    char current_header_key[1024];
    int current_header_key_length;
    char current_header_value[1024];
    int current_header_value_length;
    int keep_alive;

    int psocket;
    int epsocket;
    int replied;


    struct http_routes * routes;

    /* setup timers */
    uv_timer_t * timeout;
    uv_timer_t * interval;

    /* counters, useful to release itself
       when no ter or coutner is especting us */
    int refcount;
    void (*free)( void * );
};

struct http_routes {
    int method;
    char * path;
    int (*handler)( struct http_connection * );
};

typedef struct Config Config_t;
typedef struct http_body http_body_t;
typedef struct http_request http_request_t;
typedef struct http_response http_response_t;
typedef struct http_connection http_connection_t;
typedef struct http_routes http_routes_t;
typedef (db_callback_t)(void *, json_t *);

extern Config_t * config_init(int, char **);

void config_destroy(Config_t * config);

#define zfree(x)        if (x) { free(x); x = NULL; }
#define zmalloc         malloc
#define zcalloc(x)      calloc(x, 1)
#define PANIC(x)        do { printf x; printf("\n"); fflush(stdout); exit(-1); } while(0);

#define INCREF(x)   ++x->refcount;
#define ADDREF      INCREF
#define DECREF(x)   if (x && --x->refcount == 0) { \
        if (x->free) x->free(x); \
        else zfree(x); \
        x = NULL; \
    }

#define MALLOC_EX(t, n) n = (t *)malloc(sizeof(t))
#define MALLOC(t, n)    t * MALLOC_EX(t, n)

#define FOREACH(array, key, value) do { \
    dictIterator * di = dictGetIterator(array); \
    dictEntry *de; \
    char * key; \
    char * value; \
    while((de = dictNext(di)) != NULL)  { \
        key     = (char *)dictGetKey(de); \
        value   = (char *)dictGetVal(de);

#define END     } \
    dictReleaseIterator(di); \
    } while (0);

#define FETCH_CONNECTION            http_connection_t* conn = (http_connection_t*)parser->data;
#define FETCH_CONNECTION_UV         http_connection_t* conn = (http_connection_t*)handle->data;
#define HEADER(a, b)                dictAdd(conn->response->headers, a, b);
#define HTTP_ANY                    -1
#define WEB_HANDLER(env)            _http_request_on_##env
#define WEB_EVENT(event)            static int WEB_HANDLER(event)(http_parser * parser, const char *at, size_t length) 
#define WEB_SIMPLE_EVENT(event)     static int WEB_HANDLER(event)(http_parser * parser)
#define DECLARE_WEB_EVENT(s)        parser_settings.on_##s = WEB_HANDLER(s)
#define RESPONSE_TRUE(name)             json_object_set_new(conn->response->object, name, json_true())
#define RESPONSE_FALSE(name)            json_object_set_new(conn->response->object, name, json_false())
#define RESPONSE_STRING(name, value)    json_object_set_new(conn->response->object, name, json_string(value))
#define RESPONSE_SET(name, value)       json_object_set_new(conn->response->object, name, value)

#define MIN(a, b)                           (a > b ? b : a)
#define BEGIN_ROUTES(routes)                static http_routes_t routes[] = {
#define ROUTE(url, callback)                ROUTE_EX(HTTP_ANY, url, callback)
#define ROUTE_EX(method, url, callback)     { method, url, callback },
#define END_ROUTES                          { NULL, NULL, NULL} };

#define _STRMAP(x, fnc) do { \
    unsigned char * tmp = (unsigned char *) x; \
    while (*tmp) { \
        *tmp = fnc(*tmp); \
        tmp++; \
    } \
} while (0); 

#define TOUPPER(x) _STRMAP(x, toupper);
#define TOLOWER(x) _STRMAP(x, tolower);

#endif
