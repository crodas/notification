#include "notifd.h"

Config_t * config;
dict * db;
uv_rwlock_t numlock;


#define LOCK        uv_rwlock_wrlock(&numlock);
#define RELEASE     uv_rwlock_wrunlock(&numlock);

typedef struct {
    char * id;
    json_t * result;
    db_callback_t * callback;
    void * data;
} query_t;

typedef struct {
    char * channel;
    json_t * data;
} save_t;

int database_destroy(Config_t * env)
{
    /* bye bye in memory database */
    dictRelease(db);
}

int database_init(Config_t * env)
{
    db = dictCreate(&dictTypeMemDatabase, NULL);
    config = env;
    uv_rwlock_init(&numlock);
}

static void database_worker_save_release(uv_work_t * req, int s)
{
    save_t * saved = (save_t *) req->data;
    json_decref(saved->data);
    zfree(saved->channel);
    zfree(saved);
    zfree(req);
}

static void database_worker_save(uv_work_t * req)
{
    save_t * saving = (save_t *) req->data;
    json_t * obj = saving->data;
    json_t * result;
    short short_insert = 0;

    LOCK
        if (!(result = dictFetchValue(db, saving->channel))) {
            result = json_array();
            short_insert = 1;
        }
        json_array_append_new(result, obj);
        if (json_array_size(result) > config->web_history) {
            json_array_remove(result, 0);
        }
        json_incref(obj);
        if (short_insert) {
            dictAdd(db, saving->channel, result);
        }
    RELEASE
}

void database_save(char * channel, json_t * object)
{
    MALLOC(uv_work_t, worker);
    MALLOC(save_t, saving);
    saving->channel = strdup(channel);
    saving->data = object;
    worker->data = (void *)saving;
    json_incref(object);

    uv_queue_work(uv_default_loop(), worker, database_worker_save, database_worker_save_release);
}

static void database_worker_query_release(uv_work_t * req, int s)
{
    query_t * q = (query_t *)req->data;
    q->callback(q->data, q->result);
    zfree(q);
    zfree(req);
}

void database_worker_query(uv_work_t *req)
{
    query_t * q = (query_t *)req->data;
    LOCK
    q->result = (json_t * ) dictFetchValue(db, q->id);
    if (q->result) json_incref(q->result);
    RELEASE
}

void database_query(char * id, db_callback_t * next, void * data)
{
    MALLOC(uv_work_t, worker);
    MALLOC(query_t, query);

    query->id = id;
    query->data = data;
    query->callback = next;

    worker->data = (void *)query;

    uv_queue_work(uv_default_loop(), worker, database_worker_query, database_worker_query_release);
}

