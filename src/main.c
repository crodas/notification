#include "notifd.h"

static void do_exit(int sig)
{
    uv_stop(uv_default_loop());
}

int main(int argc, char ** argv)
{
    Config_t * config;

    signal(SIGINT, do_exit); 
    config = config_init(argc, argv);
    if (!config) {
        PANIC(("Cannot load settings"));
    }
    printf("Host: %s:%d\n", config->web_ip, config->web_port);


    webserver_init(config, webroutes_get());
    pubsub_init(config);
    database_init(config);


    uv_run(uv_default_loop(), UV_RUN_DEFAULT); /* main loop! */

    database_destroy(config);
    pubsub_destroy(config);
    config_destroy(config);
}
