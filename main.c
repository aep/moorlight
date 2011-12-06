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
#define PLUGIN_RUN(fun, call) { \
    struct dc_plugin *plugin = dc_plugins; \
    while (plugin) { \
        if (!plugin->fun) { \
            plugin = plugin->next; \
            continue; \
        } \
        (*plugin->fun)call; \
        plugin = plugin->next; \
    } \
} \

#else
#define PLUGIN_RUN(fun, call) \
    if (iaminit)  sysv_ ## fun call; \
    process_ ## fun call; \
    meubus_ ## fun call; \
    cgroup_ ## fun call; \

#endif




static int iaminit = 0;

#ifdef DYNAMIC_PLUGINS
static struct dc_plugin *dc_plugins = 0;
#endif

void dc_start_child(struct task *task)
{
    PLUGIN_RUN(prepare_child, (task));
    PLUGIN_RUN(exec, (task));
    // if we're here, no plugin could do an exec
    log_error("dc", "no plugin can start task %s", task->name);
}

void dc_start_parent(struct task *task)
{
    PLUGIN_RUN(prepare_parent, (task));
}

int dc_start(struct task *task)
{
    if (task->running != 0 )
        return 0;
    task->running = 1;
    PLUGIN_RUN(prepare, (task));
    assume(task->pid = fork());
    if (task->pid == 0) {
        dc_start_child(task);
    } else if (task->pid > 0){
        dc_start_parent(task);
    }
}
int dc_stop(struct task *task)
{
    if (task->running == 0 )
        return 0;
    PLUGIN_RUN(stop, (task));
    task->running = 0;
    return 0;
}

int dc_restart(struct task *task)
{
    dc_stop(task);
    dc_start(task);
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
    PLUGIN_RUN(register_group, (group));
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
    PLUGIN_RUN(register, (task));
    return 0;
}

int dc_unregister(struct task_group *group, struct task *task)
{
    PLUGIN_RUN(unregister, (task));

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



#ifdef DYNAMIC_PLUGINS
    //TODO: not very dynamic...
    dc_plugins = &process_plugin;
    dc_plugins->next = &cgroup_plugin;
#endif

    PLUGIN_RUN(init, ());

    log_info("dc", "Moorlight 1 booting up");

    struct task_group *default_group = calloc(1,  sizeof(struct task_group));
    default_group->name = "default";
    dc_register_group(default_group);

    //test
    /*
    struct task *test = calloc(1, sizeof(struct task));
    test->name = strdup("test");
    test->cmd  = strdup("/home/aep/proj/moorlight/test/testdaemon.sh");
    dc_register(default_group, test);
    */

    for (struct task_group *i = task_groups; i ; i = i->next)
        for (struct task *j = i->tasks; j ; j = j->next)
            dc_start(j);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        PLUGIN_RUN(select, (&rfds, &maxfd));

        int ret = 0;
        do {
            int ret = select(maxfd + 2, &rfds, NULL, NULL, NULL);
        } while (ret == EINTR);

        if (ret < 0) {
            assume(ret);
        }

        log_debug("dc", "activated %i ", ret);

        PLUGIN_RUN(activate, (&rfds));
    }

    for (struct task_group *i = task_groups; i ; i = i->next)
        for (struct task *j = i->tasks; j ; j = j->next)
            dc_stop(j);


    for (struct task_group *i = task_groups; i ; i = i->next) {
        for (struct task *j = i->tasks; j ; j = j->next) {
            dc_unregister(i, j);
        }
        PLUGIN_RUN(unregister_group, (i));
    }

    PLUGIN_RUN(teardown, ());


    return 0;
}


