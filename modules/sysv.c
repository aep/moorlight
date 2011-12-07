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

struct sysv_entry
{
    struct sysv_entry *next;
    char *id;
    char *runlevel;
    char *cmd;
};

static struct task_group *sysv_group = 0;

int sysv_init()
{

    sysv_group = dc_new_group("sysv");
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
            struct task *task = dc_new_task(sysv_group, id);
            task->cmd = cmd;
        } else {
            struct sysv_entry *entry = calloc(1, sizeof(struct sysv_entry));
            entry->id =  strdup(id);
            entry->runlevel = strdup(runlevel);
            entry->cmd = strdup(cmd);
            if (strcmp(action, "sysinit") == 0) {
                //FIXME: ugh
                system(cmd);
            } else if (strcmp(action, "wait") == 0) {
            }
        }

    }
    log_info("sysv", "default runlevel is %s", default_runlevel);
    return 0;
}
void sysv_teardown()
{
}
