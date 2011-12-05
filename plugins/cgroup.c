#include "plugin.h"
#include "logger.h"


int cgroup_init()
{
    log_debug("cgroup", "plugin initialized");
    return 0;
}

int cgroup_teardown()
{
    return 0;
}


int cgroup_register_group(struct task_group *group)
{
    log_debug("cgroup", "[%s] creating cgroup for group", group->name);
    return 0;
}

int cgroup_unregister_group(struct task_group *group)
{
    log_debug("cgroup", "[%s] removing cgroup for group", group->name);
    return 0;
}

int cgroup_register(struct task *task)
{
    log_debug("cgroup", "[%s] creating cgroup for task", task->name);
    return 0;
}

int cgroup_unregister(struct task *task)
{
    log_debug("cgroup", "[%s] removing cgroup for task", task->name);
    return 0;
}

int cgroup_prepare(struct task *task)
{
    return 0;
}

int cgroup_prepare_child(struct task *task)
{
    return 0;
}

int cgroup_prepare_parent(struct task *task)
{
    log_debug("cgroup", "[%s] adding pid %i to group", task->name, task->pid);
    return 0;
}

int cgroup_stop(struct task *task)
{
    log_debug("cgroup", "[%s] terminating group", task->name);
    return 0;
}


int cgroup_exec(struct task *task) { return 0;}
int cgroup_activate(fd_set *rfds) {return 0;}
int cgroup_select(fd_set *rfds, int *maxfd) {return 0;}

#ifdef DYNAMIC_PLUGINS
struct dc_plugin cgroup_plugin =
{
    "cgroup",
    &cgroup_init,
    &cgroup_teardown,
    &cgroup_register,
    &cgroup_unregister,
    &cgroup_prepare,
    &cgroup_prepare_child,
    &cgroup_prepare_parent,
    &cgroup_exec,
    &cgroup_stop,
    &cgroup_select,
    &cgroup_activate,
    &cgroup_register_group,
    &cgroup_unregister_group,
    0
};
#endif

