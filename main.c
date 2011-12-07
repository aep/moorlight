#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <libgen.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include "logger.h"
#include "plugins/plugin.h"


#if DYNAMIC_PLUGINS
#define PLUGIN_RUN(r, fun, call) { \
    struct dc_plugin *plugin = dc_plugins; \
    while (plugin) { \
        if (!plugin->fun) { \
            plugin = plugin->next; \
            continue; \
        } \
        r = (*plugin->fun)call; \
        if (r != 0) \
           break; \
        plugin = plugin->next; \
    } \
} \

#else
#define PLUGIN_RUN(r, fun, call) \
    if (iaminit)  sysv_ ## fun call; \
    r = process_ ## fun call; \
    if (r == 0) \
    r = meubus_ ## fun call; \
    if (r == 0) \
    r = cgroup_ ## fun call; \

#endif




static int iaminit = 0;

#ifdef DYNAMIC_PLUGINS
static struct dc_plugin *dc_plugins = 0;
#endif

void dc_start_child(struct task *task)
{
    int r = 0;
    PLUGIN_RUN(r, prepare_child, (task));
    PLUGIN_RUN(r, exec, (task));
    // if we're here, no plugin could do an exec
    log_error("dc", "no plugin can start task %s", task->name);
    exit(r);
}

int dc_start_parent(struct task *task)
{
    int r = 0;
    PLUGIN_RUN(r, prepare_parent, (task));
    return r;
}

int dc_start(struct task *task)
{
    int r = 0;
    if (task->running != 0 )
        return 0;
    task->running = 1;
    PLUGIN_RUN(r, prepare, (task));
    if (r) {
        log_error("dc", "task prepare failed for %s", task->name);
        task->running = 0;
        return r;
    }
    assume(task->pid = fork());
    if (task->pid == 0) {
        dc_start_child(task);
    } else if (task->pid > 0){
        r = dc_start_parent(task);
    } else {
        r = errno;
    }
    if (r) {
        log_error("dc", "task starting failed for %s", task->name);
        task->running = 4;
        dc_stop(task);

    }
    return r;
}
int dc_stop(struct task *task)
{
    int r = 0;
    if (task->running == 0 )
        return 0;
    PLUGIN_RUN(r, stop, (task));
    task->running = 0;
    return 0;
}

int dc_restart(struct task *task)
{
    dc_stop(task);
    dc_start(task);
    return 0;
}

int dc_on_died(struct task *task)
{
    time_t now = time(0);
    if (now - task->last_died < 5) {
        if (task->toofastcounter++ > 10) {
            log_error("dc", "task %s respawing too fast. human intervention required", task->name);
            dc_stop(task);
            return 3;
        }
    }
    task->toofastcounter = 0;
    task->last_died = now;
    dc_restart(task);
    return 0;
}

struct task_group *task_groups = 0;

int dc_register_group(struct task_group *group)
{
    if (task_groups) {
        assert (task_groups->prev == 0);
        task_groups->prev = group;
        group->next = task_groups;
    }
    task_groups = group;
    int r = 0;
    PLUGIN_RUN(r, register_group, (group));
    return 0;
}

int dc_register(struct task_group *group, struct task *task)
{
    task->group = group;
    if (group->tasks) {
        assert (group->tasks->prev == 0);
        group->tasks->prev = task;
        task->next = group->tasks;
    }
    group->tasks = task;
    int r = 0;
    PLUGIN_RUN(r, register, (task));
    return 0;
}

int dc_unregister(struct task_group *group, struct task *task)
{
    int r = 0;
    PLUGIN_RUN(r, unregister, (task));

    if (task->prev)
        task->prev->next = task->next;
    if (task->next)
        task->next->prev = task->prev;
    if (group->tasks == task)
        group->tasks = task->next;

    free (task->name);
    free (task->cmd);
    free (task);

    return 0;
}

static int running = 1;
int dc_quit()
{
    running = 0;
}

#ifdef DYNAMIC_PLUGINS
extern struct dc_plugin process_plugin;
extern struct dc_plugin cgroup_plugin;
#endif

int main(int argc, char **argv)
{

    if (getuid() != 0)
        panic("sorry, must be superuser");

    if (strcmp(argv[0], "/sbin/init") == 0) {
        iaminit = 1;
        //FIXME: ugh

        system("mount -n tmpfs -t tmpfs /task");
    }

    log_info("dc", "Moorlight 1 booting up");


#ifdef DYNAMIC_PLUGINS
    //TODO: not very dynamic...
    dc_plugins = &process_plugin;
    dc_plugins->next = &cgroup_plugin;
#endif

    int r = 0;
    PLUGIN_RUN(r, init, ());


    struct task_group *default_group = calloc(1,  sizeof(struct task_group));
    default_group->name = "default";
    dc_register_group(default_group);

    //test
    struct task *test = calloc(1, sizeof(struct task));
    test->name = strdup("test");
    test->cmd  = strdup("/home/aep/proj/moorlight/test/testdaemon.sh");
    dc_register(default_group, test);

    for (struct task_group *i = task_groups; i ; i = i->next)
        for (struct task *j = i->tasks; j ; j = j->next)
            dc_start(j);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        PLUGIN_RUN(r, select, (&rfds, &maxfd));

        int ret = 0;
        do {
            int ret = select(maxfd + 2, &rfds, NULL, NULL, NULL);
        } while (ret == EINTR);

        if (ret < 0) {
            assume(ret);
        }

        log_debug("dc", "activated %i ", ret);

        PLUGIN_RUN(r, activate, (&rfds));
    }

    for (struct task_group *i = task_groups; i ; i = i->next)
        for (struct task *j = i->tasks; j ; j = j->next)
            dc_stop(j);


    for (struct task_group *i = task_groups; i ; i = i->next) {
        for (struct task *j = i->tasks; j ; j = j->next) {
            dc_unregister(i, j);
        }
        int r = 0;
        PLUGIN_RUN(r, unregister_group, (i));
    }

    PLUGIN_RUN(r, teardown, ());


    return 0;
}


