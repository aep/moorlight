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
    process_ ## fun call; \
    cgroup_ ## fun call; \
    meubus_ ## fun call; \
//    sysv_ ## fun call; \

#endif


#ifdef DYNAMIC_PLUGINS
static struct dc_plugin *dc_plugins = 0;
#endif

void dc_start_child(struct dc *dc)
{
    PLUGIN_RUN(prepare_child, (dc));
    PLUGIN_RUN(exec, (dc));
    // if we're here, no plugin could do an exec
    log_error("dc", "no plugin can start task %s", dc->name);
}

void dc_start_parent(struct dc *dc)
{
    PLUGIN_RUN(prepare_parent, (dc));
}

int dc_start(struct dc *dc)
{
    PLUGIN_RUN(prepare, (dc));
    assume(dc->pid = fork());
    if (dc->pid == 0) {
        dc_start_child(dc);
    } else if (dc->pid > 0){
        dc_start_parent(dc);
    }
}


int dc_terminate(struct dc *dc)
{
    PLUGIN_RUN(terminate, (dc));
    return 0;
}


int dc_restart(struct dc *dc)
{
    dc_terminate(dc);
    dc_start(dc);
    return 0;
}


dc_list *dcl = 0;
int dc_register(struct dc *dc)
{
    if (dcl) {
        dc->next = dcl;
    }
    dcl = dc;
    PLUGIN_RUN(register, (dc));
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
#ifdef DYNAMIC_PLUGINS
    //TODO: not very dynamic...
    dc_plugins = &process_plugin;
    dc_plugins->next = &cgroup_plugin;
#endif

    PLUGIN_RUN(init, ());

    log_info("dc", "Moorlight 1 booting up");


    //test
    struct dc dc;
    dc.name = "test";
    dc.cmd =  "/home/aep/proj/moorlight/test/testdaemon.sh";
    dc.next = 0;
    dc_register(&dc);


    struct dc *dci = dcl;
    while (dci) {
        dc_start(dci);
        dci = dci->next;
    }

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        PLUGIN_RUN(select, (dcl, &rfds, &maxfd));

        int ret = 0;
        do {
            int ret = select(maxfd + 2, &rfds, NULL, NULL, NULL);
        } while (ret == EINTR);

        if (ret < 0) {
            assume(ret);
        }

        fprintf(stderr, "activated %i \n", ret);

        PLUGIN_RUN(activate, (dcl, &rfds));
    }

    dci = dcl;
    while (dci) {
        dc_terminate(dci);
        dci = dci->next;
    }
    dci = dcl;
    while (dci) {
        PLUGIN_RUN(unregister, (dci));
        dci = dci->next;
    }
    PLUGIN_RUN(teardown, ());


    return 0;
}


