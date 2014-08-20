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
#include <sys/stat.h>

#define _config_string(x)           strdup(x)
#define _config_integer(x)          atoi(x)
#define _config_true(x)             1     

#define FROM_ARGV(var, type, t) do { \
    int x; \
    for (x = 1; x < argc; x++) { \
        if (strcmp(#type, "true") == 0 && argv[x][0] == '-' && strcmp(argv[x]+1, #t) == 0) { \
            var = _config_##type(argv[x]); \
            break; \
        } \
        else if (argv[x][0] == '-' && strcmp(argv[x]+1, #t) == 0 && ++x <= argc) { \
            var = _config_##type(argv[x]); \
            break; \
        } \
    } \
} while(0); 

#define CONFIG_SET_IF_STR(name, default) do { \
    env->name = NULL; \
    FROM_ARGV(env->name, string, name) \
    if (env->name == NULL) {\
        json_t * t; \
        t = json ? json_object_get(json, #name) : NULL; \
        if (t && json_is_string(t)) { \
            env->name = strdup(json_string_value(t)); \
        }\
        CONFIG_SET_DEFAULT(name, strdup(default)) \
    } \
} while (0);

#define json_true_value(x)     1
#define CONFIG_SET_IF(type, name, default) do { \
    env->name = 0; \
    FROM_ARGV(env->name, type, name) \
    if (env->name == 0) { \
        json_t * t; \
        env->name = default; \
        t = json ? json_object_get(json, #name) : NULL; \
        if (t && json_is_##type(t)) { \
            env->name = json_##type##_value(t); \
        } \
    }\
} while (0);

#define CONFIG_SET_DEFAULT(name, value) \
    if (!env->name) env->name = value;

static json_t * config_load_json(int argc, char **argv)
{
    int s, i;
    json_t * json;
    json_error_t error;
    struct stat sx;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '-' && ++i) continue;
        if (stat(argv[i], &sx) == -1) continue;

        json = json_load_file(argv[i], JSON_REJECT_DUPLICATES | JSON_DECODE_ANY, &error);
        if (json) return json;
    }

    return NULL;
}

void config_destroy(Config_t * config)
{
    zfree(config->web_ip);
    zfree(config->web_allow_origin);
    zfree(config->db_path);
    zfree(config);
}

Config_t * config_init(int argc, char ** argv)
{
    json_t * json = config_load_json(argc, argv);

    MALLOC(Config_t, env);


    /* default values */
    env->requests   = 0;
    env->uv_loop    = uv_default_loop();
    env->start_time = timems();

    CONFIG_SET_IF(integer, web_timeout, 30000)
    CONFIG_SET_IF(integer, web_port, 8000)
    CONFIG_SET_IF(integer, web_history, 20)
    CONFIG_SET_IF(true, debug, 0)
    CONFIG_SET_IF_STR(web_ip, "0.0.0.0")
    CONFIG_SET_IF_STR(web_allow_origin, "*")
    CONFIG_SET_IF_STR(db_path, "./db")

    json_decref(json);

    return env;
}

#undef json_true_value
