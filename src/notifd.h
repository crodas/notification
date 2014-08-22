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
#include "adlist.h"
#include <jansson.h>
#include "refcount.h"

typedef unsigned long long int uint64;

#define utime_t         double

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

    short       debug;
    char        *db_path;

    utime_t     start_time;
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
    char * uri;
    dict * args;

    struct http_body body;
    void * data;
    void (*free_data)(void *);

    int http_major;
    int http_minor;
};

struct http_connection
{
    HAS_REFERENCE;

    uv_tcp_t        stream;
    http_parser     parser;
    uv_write_t      write_req;

    utime_t time;

    struct http_request * request;
    struct http_response * response;

    int last_was_value;
    char current_header_key[1024];
    int current_header_key_length;
    char current_header_value[1024];
    int current_header_value_length;
    int keep_alive;

    char id[40];

    int replied;


    struct http_routes * routes;

    /* setup timers */
    uv_timer_t * timeout;
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

extern void config_destroy(Config_t * config);
extern utime_t timems();

#define zfree(x)        if (x) { free(x); }
#define zmalloc         malloc
#define zcalloc(x)      calloc(x, 1)
#define PANIC(x)        do { printf x; printf("\n"); fflush(stdout); exit(-1); } while(0);

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

#define $_GET(x)                        dictFetchValue(conn->request->args, x);

#define BETWEEN(a, min, max)                (a >= min && max >= a)
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

#define _assertWithInfo(expr, file, line)   printf("Assertion failed: %s at %s:%d\n", expr, file, line)

#define assertWithInfo(x)   ((x) ? (void)0 : (_assertWithInfo(#x, __FILE__, __LINE__), exit(1)) )

#endif
