#include <unistd.h>
#include <sys/select.h>
#include <ubus.h>

struct dc
{
    const char *name;
    const char *cmd; //replace with config
    int state;
    pid_t pid;
    int proccom[2];
    int running;
    ubus_t *bus_restart;
    ubus_t *bus_start;
    ubus_t *bus_stop;
    ubus_t *bus_status;

    struct dc *next;
};

typedef struct dc dc_list;
extern dc_list *dcl;

typedef int (*dc_plugin_init)();
typedef int (*dc_plugin_teardown)();
typedef int (*dc_plugin_register)       (struct dc *dc);
typedef int (*dc_plugin_unregister)     (struct dc *dc);
typedef int (*dc_plugin_prepare)        (struct dc *dc);
typedef int (*dc_plugin_prepare_child)  (struct dc *dc);
typedef int (*dc_plugin_prepare_parent) (struct dc *dc);
typedef int (*dc_plugin_exec)           (struct dc *dc);
typedef int (*dc_plugin_terminate)      (struct dc *dc);
typedef int (*dc_plugin_select)         (dc_list *dcs, fd_set *, int *maxfd);
typedef int (*dc_plugin_activate)       (dc_list *dcs, fd_set *);

struct dc_plugin
{
    const char *name;
    dc_plugin_init init;
    dc_plugin_teardown teardown;
    dc_plugin_register reg;
    dc_plugin_unregister unreg;
    dc_plugin_prepare prepare;
    dc_plugin_prepare_child prepare_child;
    dc_plugin_prepare_parent prepare_parent;
    dc_plugin_exec exec;
    dc_plugin_terminate terminate;
    dc_plugin_select select;
    dc_plugin_activate activate;

    struct dc_plugin *next;
};
