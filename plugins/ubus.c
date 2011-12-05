#include "plugin.h"
#include "logger.h"

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <ubus.h>

static ubus_t *service;

int meubus_init()
{
    service = ubus_create("/var/ubus/moorlight/controller");
    if (!service) {
        log_error("ubus", "unable to create service bus");
    }
    log_debug("ubus", "plugin initialized");
    return 0;
}

int meubus_teardown()
{
    ubus_destroy(service);
    service = 0;
    return 0;
}

int meubus_prepare(struct dc *dc)
{
    return 0;
}

int meubus_prepare_child(struct dc *dc)
{
    return 0;
}

int meubus_prepare_parent(struct dc *dc)
{
    const char *path = "/var/ubus/moorlight/tasks/";
    char *nam = malloc(strlen(path) + strlen(dc->name) + 14);
    strcpy(nam, path);
    strcat(nam, "/");
    strcat(nam, dc->name);
    strcat(nam, "/");
    int e = strlen(nam);

    strcat(nam, "start");
    dc->bus_start   = ubus_create(nam);
    nam[e] = 0;
    strcat(nam, "stop");
    dc->bus_stop    = ubus_create(nam);
    nam[e] = 0;
    strcat(nam, "status");
    dc->bus_status  = ubus_create(nam);
    nam[e] = 0;
    strcat(nam, "pid");
    FILE *pidfile = fopen(nam, "w");
    fprintf(pidfile, "%i\n", dc->pid);
    fclose(pidfile);

    free(nam);

    return 0;
}

int meubus_terminate(struct dc *dc)
{
    log_error("ubus", "tearing down dc bus");
    ubus_destroy(dc->bus_start);
    ubus_destroy(dc->bus_status);
    ubus_destroy(dc->bus_stop);

    const char *path = "/var/ubus/moorlight/tasks/";
    char *nam = malloc(strlen(path) + strlen(dc->name) + 14);
    strcpy(nam, path);
    strcat(nam, "/");
    strcat(nam, dc->name);
    strcat(nam, "/");
    int e = strlen(nam);

    strcat(nam, "pid");
    unlink(nam);

    nam[e] = 0;
    rmdir(nam);

    return 0;
}

int meubus_exec(struct dc *dc)
{
    return 0;
}

int meubus_select (dc_list *dc, fd_set *rfds, int *maxfd)
{
    int r = ubus_select_all (service, rfds);
    if (r > *maxfd)
        *maxfd = r;
    return 0;
}

int meubus_activate(dc_list *dc, fd_set *rfds)
{
    ubus_activate_all (service, rfds, 0);
    ubus_chan_t * chan=0;
    while ((chan=ubus_ready_chan(service))) {
        char buff [1024];
        int len = ubus_read(chan, &buff, 1024);
        if (len < 1) {
            ubus_disconnect(chan);
        } else {
            buff[len] = 0;
            char *line = strtok(buff, "\n\r");
            while (line) {
                fprintf(stderr, ")))))%s(((\n", line);

                char *command = strtok(line, "\t");
                if (strcmp(command, "run") == 0) {
                    char *name = strtok(NULL, "\t");
                    char *cmd  = strtok(NULL, "\t");
                    if (!cmd) {
                        const char *resp = "\e32\tinvalid argument\033[0m\n";
                        ubus_write(chan, resp, strlen(resp));
                        ubus_disconnect(chan);
                        break;
                    }
                    struct dc *dc = malloc(sizeof(struct dc));
                    dc->name = strdup(name);
                    dc->cmd =  strdup(cmd);
                    dc->next = 0;
                    dc_add(dc);
                    log_info("ubus", "adding dc %s", cmd);

                    const char *resp;
                    if (dc_start(dc) == 0) {
                        resp = "started\n";
                    } else {
                        resp = "\e32\tunable to start\n";
                    }
                    ubus_write(chan, resp, strlen(resp));
                    ubus_disconnect(chan);
                    return ;
                } else {
                    const char *resp = "\e31\tunknown command\n";
                    ubus_write(chan, resp, strlen(resp));
                    ubus_disconnect(chan);
                    return;
                }

                line = strtok(NULL,"\n\r");
            }
        }
    }

    return 0;
}

#ifdef DYNAMIC_PLUGINS
struct dc_plugin meubus_plugin =
{
    "meubus",
    &meubus_init,
    &meubus_teardown,
    &meubus_prepare,
    &meubus_prepare_child,
    &meubus_prepare_parent,
    &meubus_exec,
    &meubus_terminate,
    &meubus_select,
    &meubus_activate,
    0
};
#endif

