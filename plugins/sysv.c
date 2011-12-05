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



struct sysv_entry
{
    struct sysv_entry *next;
    char *id;
    char *runlevel;
    char *cmd;
};

int sysv_init()
{

    struct sysv_entry *entries = 0;

    log_debug("sysv", "plugin initialized");
    FILE *inittab = fopen("/etc/inittab", "r");
    if (!inittab) {
        log_error("sysv", "cannot open /etc/inittab");
        return 1;
    }

    const char *default_runlevel = "3";
    char buf[2048];
    while (fgets(buf, sizeof(buf), inittab)) {
        char *line = buf;
        if (strchr(line, '#')) {
            continue;
        }

        line[strlen(line) -1] = 0;

        char *id      = strsep(&line, ":");
        if (!id) continue;
        char *runlevel= strsep(&line, ":");
        if (!runlevel) continue;
        char *action  = strsep(&line, ":");
        if (!action) continue;
        char *cmd     = strsep(&line, ":");
        if (!cmd) continue;
        fprintf(stderr, " -< %s . %s . %s . %s\n", id, runlevel, action , cmd);

        if (strcmp(action, "initdefault") == 0) {
            default_runlevel = strdup(runlevel);
        } else if (strcmp(action, "respawn") == 0) {
            log_info("sysv", "adding dc %s", cmd);
            struct dc *dc = malloc(sizeof(struct dc));
            dc->name = strdup(id);
            dc->cmd =  strdup(cmd);
            dc->next = 0;
            dc_register(dc);
        } else {
            struct sysv_entry *entry = malloc(sizeof(struct sysv_entry));
            entry->id =  strdup(id);
            entry->runlevel = strdup(runlevel);
            entry->cmd = strdup(cmd);
            if (strcmp(action, "sysinit") == 0) {
            } else if (strcmp(action, "wait") == 0) {
            }
        }

    }
    log_info("sysv", "default runlevel is %s", default_runlevel);
    return 0;
}

int sysv_teardown()
{
    return 0;
}
int sysv_register(struct dc *dc)
{
    return 0;
}
int sysv_unregister(struct dc *dc)
{
    return 0;
}

int sysv_prepare(struct dc *dc)
{
    return 0;
}

int sysv_prepare_child(struct dc *dc)
{
    return 0;
}

int sysv_prepare_parent(struct dc *dc)
{
    return 0;
}

int sysv_terminate(struct dc *dc)
{
    return 0;
}

int sysv_exec(struct dc *dc)
{
    return 0;
}

int sysv_select (dc_list *dc, fd_set *rfds, int *maxfd)
{
    return 0;
}

int sysv_activate(dc_list *dc, fd_set *rfds)
{
    return 0;
}

#ifdef DYNAMIC_PLUGINS
struct dc_plugin sysv_plugin =
{
    "sysv",
    &sysv_init,
    &sysv_teardown,
    &sysv_register,
    &sysv_unregister,
    &sysv_prepare,
    &sysv_prepare_child,
    &sysv_prepare_parent,
    &sysv_exec,
    &sysv_terminate,
    &sysv_select,
    &sysv_activate,
    0
};
#endif
