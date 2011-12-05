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


int cgroup_register(struct dc *dc)
{
    log_debug("cgroup", "[%s] creating cgroup", dc->name);
    return 0;
}

int cgroup_unregister(struct dc *dc)
{
    log_debug("cgroup", "[%s] removing cgroup", dc->name);
    return 0;
}

int cgroup_prepare(struct dc *dc)
{
    return 0;
}

int cgroup_prepare_child(struct dc *dc)
{
    return 0;
}

int cgroup_prepare_parent(struct dc *dc)
{
    log_debug("cgroup", "[%s] adding pid %i to group", dc->name, dc->pid);
    return 0;
}

int cgroup_terminate(struct dc *dc)
{
    log_debug("cgroup", "[%s] terminating group", dc->name);
    return 0;
}


int cgroup_exec(struct dc *dc) { return 0;}
int cgroup_activate(struct dc *dc, fd_set *rfds) {return 0;}
int cgroup_select(struct dc *dc, fd_set *rfds, int *maxfd) {return 0;}

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
    &cgroup_terminate,
    &cgroup_select,
    &cgroup_activate,
    0
};
#endif

