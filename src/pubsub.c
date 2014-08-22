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
#include "adlist.h"
#include <nanomsg/nn.h>
#include <nanomsg/reqrep.h>
#include <nanomsg/pubsub.h>
#include <nanomsg/pair.h>

struct pubsub_channel
{
    HAS_REFERENCE
    list * clients;
};

dict * pubsub_channels;

static uv_loop_t loop;

static char pubsub_name[40];
static int sock;
static int esock;


int pubsub_destroy(Config_t * env)
{
    nn_shutdown(sock, esock);
    nn_close(sock);
    dictRelease(pubsub_channels);
}

int pubsub_init(Config_t *env)
{
    char tmp[200];
    int val = 1;

    sock = nn_socket (AF_SP, NN_PUB);
    strcpy(pubsub_name, "inproc://pubsub");
    if ((esock = nn_bind(sock, pubsub_name)) == -1) {
        PANIC(("Failed to listen internal pubsub server (%s)", pubsub_name));
    }

    pubsub_channels = dictCreate(&setDictListType, NULL);
}



static void database_save_handler(char * channel, json_t * object)
{
    if (1) {
        char * t = json_dumps(object, 0);
        //printf("Setting %s => %s\n", channel, t);fflush(stdout);
        free(t);
    }

    dictEntry *de;
    listNode *ln;
    listIter li;

    de = dictFind(pubsub_channels, channel);
    if (de) {
        list *list = dictGetVal(de);
        listNode *ln;
        listIter li;

        listRewind(list,&li);
        while ((ln = listNext(&li)) != NULL) {
            http_connection_t * conn = (http_connection_t*)ln->value;
            if (conn->replied) continue;
            json_t * messages = json_array();
            json_array_append(messages, object);
            RESPONSE_SET("messages", messages);
            RESPONSE_SET("n", json_integer(1));
            RESPONSE_FALSE("db");
            RESPONSE_TRUE("pubsub");
            http_send_response(conn);
        }
    }
}

int pubsub_publish(char * channel, json_t * object)
{
    char  uuid_str[32];

    get_unique_id(uuid_str);
    json_object_set_new(object, "_id", json_string(uuid_str));
    json_object_set_new(object, "_ack", json_false());
    json_incref(object);
    database_save(channel, object, &database_save_handler);
}

void pubsub_subscription_destroy(http_connection_t * conn)
{
    char * channel, * value;
    dict * subs = (dict *)conn->request->data;
    dictEntry * de;
    list * clients;
    listNode * ln;

    FOREACH(subs, channel, value)
        de = dictFind(pubsub_channels, channel);
        assertWithInfo(de != NULL);
        clients = (list *)dictGetVal(de);
        ln = listSearchKey(clients, conn);
        assertWithInfo(ln != NULL);
        listDelNode(clients, ln);
        if (listLength(clients) == 0) { 
            assertWithInfo(dictDelete(pubsub_channels, channel) == DICT_OK);
        }
    END
    dictRelease(subs);
}

int pubsub_subscription_create(char * channel, http_connection_t * conn)
{
    if (conn->request->data == NULL) {
        conn->request->data = dictCreate(&setDictType, NULL);
        conn->request->free_data = &pubsub_subscription_destroy;
    }

    dict * subs = (dict*)conn->request->data;
    dictEntry * de;
    list * clients = NULL;

    if (dictAdd(subs, channel, NULL) == DICT_OK) {
        de = dictFind(pubsub_channels, channel);
        if (de == NULL) {
            clients = listCreate();
            dictAdd(pubsub_channels, channel, clients);
        } else {
            clients = dictGetVal(de);
        }
        listAddNodeTail(clients, conn);
    }

}
