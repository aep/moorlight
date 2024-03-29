#include "main.h"
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


static ubus_t *bus_newgroup;

int mod_ubus_init()
{
    //TODO: should log it as error.
    signal(SIGPIPE, SIG_IGN);

    bus_newgroup = ubus_create("/task/new");
    if (!bus_newgroup) {
        log_error("ubus", "unable to create bus /task/new: %s", strerror(errno));
    }
    log_debug("ubus", "plugin initialized");
    return 0;
}
void mod_ubus_teardown()
{
    if (bus_newgroup)
        ubus_destroy(bus_newgroup);
    bus_newgroup = 0;
}



int mod_ubus_new_group(struct task_group *group)
{
    const char *path = "/task";

    char *nam = calloc(strlen(path) + strlen(group->name) + 14, sizeof(char));
    group->servicepath = nam;
    strcpy(nam, path);
    strcat(nam, "/");
    strcat(nam, group->name);
    strcat(nam, "/");
    int e = strlen(nam);

    log_info("ubus", "creating group object %s", nam);

    strcat(nam, "new");
    group->bus_new = ubus_create(nam);
    nam[e] = 0;

    if (!group->bus_new)
        log_error("ubus", "unable to create bus %s: %s", nam, strerror(errno));

    return 0;
}
void mod_ubus_delete_group(struct task_group *group)
{
    log_debug("eubus", "[%s] removing group object", group->name);

    if (group->bus_new)
        ubus_destroy(group->bus_new);
    group->bus_new = 0;
    if (group->servicepath) {
        rmdir(group->servicepath);
        free(group->servicepath);
    }
    group->servicepath = 0;
}



int mod_ubus_new_task(struct task *task)
{
    const char *path = task->group->servicepath;
    char *nam = calloc(strlen(path) + strlen(task->name) + 14, sizeof(char));
    task->servicepath = nam;
    strcpy(nam, path);
    strcat(nam, "/");
    strcat(nam, task->name);
    strcat(nam, "/");
    int e = strlen(nam);

    log_info("ubus", "creating object %s", nam);

    strcat(nam, "restart");
    task->bus_restart   = ubus_create(nam);
    if (!task->bus_restart)
        log_error("ubus", "unable to create bus %s: %s", nam, strerror(errno));
    nam[e] = 0;

    strcat(nam, "start");
    task->bus_start   = ubus_create(nam);
    if (!task->bus_start)
        log_error("ubus", "unable to create bus %s: %s", nam, strerror(errno));
    nam[e] = 0;

    strcat(nam, "stop");
    task->bus_stop    = ubus_create(nam);
    if (!task->bus_stop)
        log_error("ubus", "unable to create bus %s: %s", nam, strerror(errno));
    nam[e] = 0;

    strcat(nam, "status");
    task->bus_status  = ubus_create(nam);
    if (!task->bus_status)
        log_error("ubus", "unable to create bus %s: %s", nam, strerror(errno));
    nam[e] = 0;

    strcat(nam, "pid");
    FILE *pidfile = fopen(nam, "w");
    fprintf(pidfile, "\n");
    fclose(pidfile);
    nam[e] = 0;

    return 0;
}
int mod_ubus_delete_task(struct task *task)
{
    log_info("ubus", "tearing down task object");

    if (task->bus_restart)
        ubus_destroy(task->bus_restart);
    if (task->bus_start)
        ubus_destroy(task->bus_start);
    if (task->bus_status)
        ubus_destroy(task->bus_status);
    if (task->bus_stop)
        ubus_destroy(task->bus_stop);

    char *nam = task->servicepath;
    if (nam) {
        int e = strlen(nam);

        strcat(nam, "pid");
        unlink(nam);
        nam[e] = 0;

        rmdir(nam);
        free(nam);
    }
    task->servicepath = 0;
    return 0;
}



int mod_ubus_on_task_started(struct task *task)
{
    const char *msg = "started\n";

    if (task->bus_status)
        ubus_broadcast(task->bus_status, msg, strlen(msg));

    char *nam = task->servicepath;
    int e = strlen(nam);

    strcat(nam, "pid");
    FILE *pidfile = fopen(nam, "w");
    fprintf(pidfile, "%i\n", task->pid);
    fclose(pidfile);
    nam[e] = 0;

    char procp[256];
    snprintf(procp, 256, "/proc/%i", task->pid);
    strcat(nam, "proc");
    symlink(procp, nam);
    nam[e] = 0;

    return 0;
}
int mod_ubus_on_task_stopped(struct task *task)
{
    const char *msg = "stopped\n";
    if (task->bus_status)
        ubus_broadcast(task->bus_status, msg, strlen(msg));

    char *nam = task->servicepath;
    int e = strlen(nam);

    strcat(nam, "pid");
    unlink(nam);
    nam[e] = 0;

    strcat(nam, "proc");
    unlink(nam);
    nam[e] = 0;

    return 0;
}



int mod_ubus_select(fd_set *rfds, int *maxfd)
{
    int r = 0;
    if (bus_newgroup) {
        r = ubus_select_all (bus_newgroup, rfds);
        if (r > *maxfd) *maxfd = r;
    }

    for (struct task_group *i = task_groups; i ; i = i->next) {

        if (i->bus_new) {
            r = ubus_select_all (i->bus_new, rfds);
            if (r > *maxfd) *maxfd = r;
        }

        for (struct task *task = i->tasks; task ; task = task->next) {

            if (task->bus_restart) {
                r = ubus_select_all (task->bus_restart, rfds);
                if (r > *maxfd) *maxfd = r;
            }
            if (task->bus_start) {
                r = ubus_select_all (task->bus_start, rfds);
                if (r > *maxfd) *maxfd = r;
            }
            if (task->bus_stop) {
                r = ubus_select_all (task->bus_stop, rfds);
                if (r > *maxfd) *maxfd = r;
            }
            if (task->bus_status) {
                r = ubus_select_all (task->bus_status, rfds);
                if (r > *maxfd) *maxfd = r;
            }

        }
    }
    return 0;
}
int mod_ubus_activate(fd_set *rfds)
{
    //TODO: implement new group call or use inotify on mkdir
    if (bus_newgroup) {
        ubus_activate_all (bus_newgroup, rfds, 1);
    }


    for (struct task_group *i = task_groups; i ; i = i->next) {
        // ------  new task
        if (i->bus_new) {
            ubus_activate_all (i->bus_new, rfds, 0);
            ubus_chan_t * chan=0;
            while ((chan=ubus_ready_chan(i->bus_new))) {
                char buff [1024];
                int len = ubus_read(chan, &buff, 1024);
                if (len < 1) {
                    ubus_disconnect(chan);
                } else {
                    buff[len] = 0;
                    char *line = strtok(buff, "\n\r");

                    char *name = strtok(line, "\t");
                    char *cmd  = strtok(NULL, "\t");
                    if (!cmd) {
                        const char *resp = "\e32\tinvalid argument\033[0m\n";
                        ubus_write(chan, resp, strlen(resp));
                        ubus_disconnect(chan);
                        break;
                    }
                    log_info("ubus", "adding dc '%s'", cmd);
                    struct task *task = dc_new_task(i, name);
                    task->cmd = cmd;

                    const char *resp;
                    if (!task) {
                        resp = "\e44\tregister failed\n";
                    } else if (dc_task_start(task) == 0) {
                        resp = "started\n";
                    } else {
                        resp = "\e32\tunable to start\n";
                    }
                    ubus_write(chan, resp, strlen(resp));
                    ubus_disconnect(chan);
                    line = strtok(NULL,"\n\r");
                }
            }
        }

        // ----- task object calls
        for (struct task *task = i->tasks; task ; task = task->next) {

            int len = 0;
            char buff [1024];
            ubus_chan_t * chan = 0;

            if (task->bus_restart) {
                ubus_activate_all (task->bus_restart, rfds, 0);
                if (chan = ubus_ready_chan(task->bus_restart)) {
                    int len = ubus_read(chan, &buff, 1024);
                    if (len == 0) {
                        ubus_disconnect(chan);
                    } else {
                        log_info("ubus", "bus command: restart %s", task->name);
                        const char *resp;
                        if (dc_task_restart(task) == 0)
                            resp = "restarted\n";
                        else
                            resp = "failed\n";
                        ubus_write(chan, resp, strlen(resp));
                    }
                }
            }

            if (task->bus_start) {
                ubus_activate_all (task->bus_start, rfds, 0);
                if (chan = ubus_ready_chan(task->bus_start)) {
                    int len = ubus_read(chan, &buff, 1024);
                    if (len == 0) {
                        ubus_disconnect(chan);
                    } else {
                        log_info("ubus", "bus command: start %s", task->name);
                        const char *resp;
                        if (dc_task_start(task) == 0)
                            resp = "started\n";
                        else
                            resp = "failed\n";
                        ubus_write(chan, resp, strlen(resp));
                    }
                }
            }

            if (task->bus_stop) {
                ubus_activate_all (task->bus_stop, rfds, 0);
                if (chan = ubus_ready_chan(task->bus_stop)) {
                    int len = ubus_read(chan, &buff, 1024);
                    if (len == 0) {
                        ubus_disconnect(chan);
                    } else {
                        log_info("ubus", "bus command: stop %s", task->name);
                        const char *resp;
                        if (dc_task_stop(task) == 0)
                            resp = "stopped\n";
                        else
                            resp = "failed\n";
                        ubus_write(chan, resp, strlen(resp));
                    }
                }
            }

            if (task->bus_status) {
                ubus_activate_all (task->bus_status, rfds, 1);
            }
        }
    }

    return 0;
}
