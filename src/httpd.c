#include "notifd.h"
#include <sys/time.h>

#define MICRO_IN_SEC 1000000.00

static Config_t     * config; 
static uv_loop_t    * uv_loop;
static http_parser_settings parser_settings;

double now()
{
    struct timeval tp = {0};
    if (gettimeofday(&tp, NULL)) {
        return -1;
    }
    return (double)(tp.tv_sec + tp.tv_usec / MICRO_IN_SEC);
}

void http_stream_on_alloc(uv_handle_t* client, size_t suggested_size, uv_buf_t* buf)
{
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void _conn_destroy(void * ptr)
{
    http_connection_t * conn = (http_connection_t *) ptr;
    zfree(conn->timeout);
    zfree(conn->interval);
    if (conn->psocket > 0) {
        pubsub_subscription_destroy(conn);
    }
    if (conn->request) {
        dictRelease(conn->request->headers);
        zfree(conn->request->body.buffer);
        zfree(conn->request->url);
        zfree(conn->request);
    }
    if (conn->response) {
        dictRelease(conn->response->headers);
        zfree(conn->response->ptr);
        json_decref(conn->response->object);
        zfree(conn->response);
    }
    zfree(conn);
}

static void _timer_close_listener(uv_handle_t* handle)
{
    FETCH_CONNECTION_UV;
    DECREF(conn);
}

static void http_stream_on_close(uv_handle_t* handle)
{
    FETCH_CONNECTION_UV;
    if (conn->timeout) {
        uv_close((uv_handle_t*)conn->timeout, _timer_close_listener);
    }
    if (conn->interval) {
        uv_close((uv_handle_t*)conn->interval, _timer_close_listener);
    }
    DECREF(conn);
}

void http_stream_on_read(uv_stream_t* handle, ssize_t nread, uv_buf_t* buf)
{
    size_t parsed;
    FETCH_CONNECTION_UV;

     if (nread >= 0) {
         parsed = http_parser_execute(&conn->parser, &parser_settings, buf->base, nread);

         if (parsed < nread) {
             /* uv_close((uv_handle_t*)
              * &client->handle,
              * http_stream_on_close);
              * */
         }
     } else {
        uv_close((uv_handle_t*) &conn->stream, http_stream_on_close);
     }
     zfree(buf->base);
}

static void on_connect(uv_stream_t* stream, int status)
{
    MALLOC(http_connection_t, conn);
    conn->request   = NULL;
    conn->response  = NULL;
    conn->time      = now();
    conn->routes    = (struct http_routes *)stream->data;
    conn->timeout   = NULL;
    conn->interval  = NULL;
    conn->psocket = 0;
    conn->refcount  = 1;
    conn->replied   = 0;
    conn->free      = _conn_destroy;

    uv_tcp_init(uv_loop, &conn->stream);
    http_parser_init(&conn->parser, HTTP_REQUEST);

    conn->write_req.data = conn;
    conn->parser.data = conn;
    conn->stream.data = conn;

    /* TODO: Use the return values from uv_accept() and
     * uv_read_start() */
    uv_accept(stream, (uv_stream_t*)&conn->stream);
    uv_read_start(
        (uv_stream_t*)&conn->stream,
        http_stream_on_alloc,
        http_stream_on_read
    );
}

WEB_SIMPLE_EVENT(message_begin)
{
    FETCH_CONNECTION;
    MALLOC_EX(http_request_t, conn->request);
    conn->request->headers = dictString(NULL);
    conn->request->body.buffer  = NULL;
    conn->request->body.len     = 0;

    /* Presize the dict to avoid rehashing */
    dictExpand(conn->request->headers, 5);

    conn->last_was_value = 0;
    conn->current_header_key_length   = 0;
    conn->current_header_value_length = 0;
    return 0;
}

static void http_build_header_ptr(http_response_t * response)
{
    int i, l1, l2;
    sprintf(
        response->header_ptr, 
        "HTTP/%d.%d %d OK\r\n",
        response->http_major, response->http_minor, response->status
    );

    i = strlen(response->header_ptr);

    FOREACH(response->headers, key, value)
        l1 = strlen(key);
        l2 = strlen(value);
        if (i + 3 +  l1 + l2 >= 4098) break;
        strncat(response->header_ptr + i, key, l1);
        i += l1;
        strncat(response->header_ptr + i, ": ", 2);
        i += 2;
        strncat(response->header_ptr + i, value, l2);
        i += l2;
        strncat(response->header_ptr + i, "\r\n", 2);
        i += 2;
    END
    strncat(response->header_ptr + i, "\r\n", 2);

    response->header_len = i + 3;
}

static void http_server_sent_body(uv_write_t* handle, int status)
{
    FETCH_CONNECTION_UV
    uv_close((uv_handle_t*)handle->handle, http_stream_on_close);
}


static void http_server_sent_header(uv_write_t* handle, int status)
{
    FETCH_CONNECTION_UV
    uv_buf_t resbuf;

    int flags;

    flags = JSON_COMPACT | JSON_ESCAPE_SLASH;

    if (conn->response->debug == 1) {
        flags |= JSON_INDENT(4);
    }

    conn->response->ptr = json_dumps(conn->response->object, flags);
    resbuf = uv_buf_init(conn->response->ptr, strlen(conn->response->ptr));

    uv_write(&conn->write_req, (uv_stream_t*) &conn->stream, &resbuf, 1, http_server_sent_body);
}

int http_send_response(http_connection_t * conn)
{
    uv_buf_t resbuf;
    char time[90];

    conn->replied = 1;

    sprintf(time, "%f", now() - conn->time);
    HEADER("X-Response-Time", time);
    HEADER("Access-Control-Allow-Origin", config->web_allow_origin);

    http_build_header_ptr(conn->response);
    resbuf = uv_buf_init(conn->response->header_ptr, conn->response->header_len);

    uv_write(&conn->write_req, (uv_stream_t*) &conn->stream, &resbuf, 1, http_server_sent_header);

    return 0;
}

int check_ip(const char * ipstr, http_connection_t * conn)
{
    struct sockaddr_in ip_addr, compare_addr;
    size_t namelen = sizeof(struct sockaddr_in);

    uv_tcp_getpeername(&conn->stream, &ip_addr, &namelen);
    uv_ip4_addr(ipstr, 0, &compare_addr);
    
    if (compare_addr.sin_family == ip_addr.sin_family) {
        return memcmp(&ip_addr.sin_addr,
            &compare_addr.sin_addr,
            sizeof compare_addr.sin_addr) == 0;
    }

    return 0;
}

http_response_t * new_response(http_request_t * req)
{
    MALLOC(http_response_t, response);
    config->requests++;
    response->http_major    = req->http_major;
    response->http_minor    = req->http_minor;
    response->status        = 200;
    response->headers       = dictString(NULL);
    response->object        = json_object();
    response->debug         = config->debug;
    response->ptr           = NULL;
    return response;
}

WEB_SIMPLE_EVENT(message_complete)
{
    FETCH_CONNECTION;
    int found = 0;
    http_routes_t * routes = (http_routes_t *)conn->routes;


    conn->response = new_response(conn->request);

    HEADER("Content-Type", "application/json");
    HEADER("Connection", "Close")
    HEADER("X-Powered-By", "WebPubSub");

    int rlen = strlen(conn->request->url);
    for (; routes->handler; routes++) {
        if (routes->method == HTTP_ANY || routes->method == conn->request->method) {
            int len = strlen(routes->path);
            int prefix = 0;

            if (routes->path[len-1] == '*') {
                prefix = 1;
                len--;
            } 
            
            if ((!prefix && len != rlen) || (prefix && len > rlen)) {
                /* they don't match */
                continue;
            }

            if (strncmp(conn->request->url, routes->path, (size_t)len) == 0) {
                routes->handler( conn );
                found = 1;
                break;
            }
                
        }
    }

    if (!found) {
        RESPONSE_TRUE("error");
        http_send_response(conn);
    }

    return 0;
}

static void _requestSetHeader(http_connection_t * conn)
{
    if (conn->last_was_value && conn->current_header_key_length > 0) {
        TOUPPER(conn->current_header_key);
        TOLOWER(conn->current_header_value);
        dictAdd(conn->request->headers, conn->current_header_key, conn->current_header_value);
        conn->current_header_key_length = 0;
        conn->current_header_value_length = 0;
    }
}

WEB_EVENT(header_field) 
{
    FETCH_CONNECTION;

    _requestSetHeader(conn);

    memcpy((char *)&conn->current_header_key[conn->current_header_key_length], at, length);
    conn->current_header_key_length += length;
    conn->current_header_key[conn->current_header_key_length] = '\0';
    conn->last_was_value = 0;

    return 0;
}

WEB_EVENT(header_value) 
{
    FETCH_CONNECTION;
    if (!conn->last_was_value && conn->current_header_value_length > 0) {
        /* Start of a new header */
        conn->current_header_value_length = 0;
    }

    memcpy((char *)&conn->current_header_value[conn->current_header_value_length], at, length);
    conn->current_header_value_length += length;
    conn->current_header_value[conn->current_header_value_length] = '\0';
    conn->last_was_value = 1;

    return 0;
}

WEB_EVENT(headers_complete)
{
    FETCH_CONNECTION;
    _requestSetHeader(conn); 

    http_request_t * req =  conn->request;
    
    req->http_major = parser->http_major;
    req->http_minor = parser->http_minor;
    req->method     = parser->method;

    return 0;
}

WEB_EVENT(url)
{
    FETCH_CONNECTION;

    struct http_parser_url u;

    char *data = (char *)malloc(sizeof(char) * length + 1);
    strncpy(data, at, length);
    data[length] = '\0';

    if (http_parser_parse_url(at, length, 0, &u) == 0) {
        //fprintf(stderr, "\n\n*** failed to parse URL %s ***\n\n", data);
        //zfree(data);
        //return -1;
    }

    
    conn->request->url = data;

    return 0;
}

WEB_EVENT(body)
{
    FETCH_CONNECTION;
    if (length > 0) {
        http_request_t * req;
        req = conn->request;

        req->body.buffer = realloc(req->body.buffer, req->body.len + length);
        memcpy(req->body.buffer + req->body.len, at, length);
        req->body.len += length;
    }
}

int webserver_init(Config_t * _config, http_routes_t * routes)
{
    DECLARE_WEB_EVENT(header_field);
    DECLARE_WEB_EVENT(header_value);
    DECLARE_WEB_EVENT(headers_complete);
    DECLARE_WEB_EVENT(message_begin);
    DECLARE_WEB_EVENT(message_complete);
    DECLARE_WEB_EVENT(body);
    DECLARE_WEB_EVENT(url);

    config = _config;


    #ifdef PLATFORM_POSIX
    signal(SIGPIPE, SIG_IGN);
    #endif // PLATFORM_POSIX

    uv_loop = uv_default_loop();
    uv_tcp_init(uv_loop, &config->web_server);
    uv_ip4_addr(config->web_ip, config->web_port, &config->web_bind);
    config->web_server.data = (void *)routes;

    uv_tcp_bind(&config->web_server, (struct sockaddr *)&config->web_bind, 0);
    uv_listen((uv_stream_t*)&config->web_server, 128, on_connect);
}

