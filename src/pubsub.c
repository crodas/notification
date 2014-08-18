#include "notifd.h"
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>

static char pubsub_name[40];
static int sock;
static int esock;
static int psock;
static int epsock;

static uv_timer_t ptimer;
static uv_loop_t loop;


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

static int _check_messages(uv_timer_t * handle)
{
    FETCH_CONNECTION_UV;

    json_t * messages = NULL;

    do {
        char * buf = NULL;
        int bytes;
        if (conn->replied) return;
        if ((bytes = nn_recv(conn->psocket, &buf, NN_MSG, NN_DONTWAIT)) == -1) {
            if (errno == EAGAIN) {
                break;
            }

            /* handle error */
            break;
        }

        if (!messages) {
            messages = json_array();
        }

        char * str = buf;
        int len = bytes;

        while (*str != '*' && *str != '\0') { len--; str++;}
        str++;len--;

        json_error_t error;
        json_t * msg = json_loadb(str, len, 0, &error);

        json_array_append_new(messages, msg);


        nn_freemsg (buf);
    } while (1);

    if (messages) {
        RESPONSE_SET("messages", messages);
        RESPONSE_SET("n", json_integer(json_array_size(messages)));
        http_send_response(conn);
    }


}

int pubsub_destroy(Config_t * env)
{
    nn_shutdown(sock, esock);
    nn_shutdown(psock, epsock);
    nn_close(sock);
    nn_close(psock);
}

int pubsub_init(Config_t *env)
{
    char tmp[200];
    int val = 1;

    sock = nn_socket (AF_SP, NN_PUB);
    strcpy(pubsub_name, "inproc://test");
    if ((esock = nn_bind(sock, pubsub_name)) == -1) {
        PANIC(("Failed to listen internal pubsub server (%s)", pubsub_name));
    }

    psock = nn_socket (AF_SP, NN_REP);
    //nn_setsockopt(psock, NN_TCP, NN_TCP_NODELAY, &val, sizeof(val));
    strcpy(tmp, "tcp://*:2508");
    if ((epsock=nn_bind(psock, tmp)) == -1) {
        PANIC(("Failed to listen public pubsub server (%s - %s)", tmp, nn_strerror(errno)));
    }

    uv_timer_init(uv_default_loop(), &ptimer);
    uv_timer_start(&ptimer, (uv_timer_cb) &_check_pubsubd, 1, 1);
}

int pubsub_publish(char * channel, json_t * object)
{
    char * buf;
    char * tmp;
    int len, tlen;
    char  uuid_str[32];

    get_unique_id(uuid_str);
    json_object_set_new(object, "_id", json_string(uuid_str));
    json_object_set_new(object, "_ack", json_false());

    if (1) {
        char * t = json_dumps(object, 0);
        printf("Setting %s\n", t);fflush(stdout);
        free(t);
    }

    database_save(channel, object);

    tmp = json_dumps(object, 0);
    tlen = strlen(tmp);
    buf = malloc(strlen(channel) + 2 + tlen);
    len = strlen(channel);
    strcpy(buf, channel);
    buf[len++] = '*';


    strcpy(buf + len, tmp);
    len += tlen;
    buf[len] = '\0';

    if (nn_send(sock, buf, len, NN_DONTWAIT) == -1) {
        PANIC(("Failed to deliver message"));
    }

    zfree(tmp);
    zfree(buf);
}

int pubsub_subscription_create(char * channel, http_connection_t * conn)
{
    int len = strlen(channel);
    char * rchannel = malloc(len + 2);
    strcpy(rchannel, channel);
    rchannel[len] = '*';
    rchannel[len+1] = '\0';

    conn->psocket = nn_socket (AF_SP, NN_SUB);
    nn_setsockopt (conn->psocket, NN_SUB, NN_SUB_SUBSCRIBE, rchannel, 0);

    if ((conn->epsocket = nn_connect(conn->psocket, pubsub_name)) == -1) {
        // TODO: Shouldn't panic, insterad should return error
        // and the connection should be closed
        PANIC(("failed to connect to pubsub server"));
    }

    conn->interval = (uv_timer_t*)malloc(sizeof(uv_timer_t));
    uv_timer_init(uv_default_loop(), conn->interval);
    conn->interval->data = (void *) conn;
    uv_timer_start(conn->interval, (uv_timer_cb) &_check_messages, 100, 100);
    ADDREF(conn);
    zfree(rchannel);
}

int pubsub_subscription_destroy(http_connection_t * conn)
{
    if (conn->psocket > 0) {
        nn_shutdown(conn->psocket, conn->epsocket);
        nn_close(conn->psocket);
        conn->psocket = 0;
        conn->epsocket = 0;
    }
}
