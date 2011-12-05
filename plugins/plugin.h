#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <ubus.h>


struct task_group;
struct task
{
    char *name;
    char *servicepath;
    char *cmd; //replace with config
    int state;
    pid_t pid;
    int proccom[2];
    int running;

    struct task_group *group;

    ubus_t *bus_restart;
    ubus_t *bus_start;
    ubus_t *bus_stop;
    ubus_t *bus_status;

    struct task *next;
    struct task *prev;

};

struct task_group
{
    char *name;
    char *servicepath;
    ubus_t *bus_new;
    struct task *tasks;

    struct task_group *next;
    struct task_group *prev;
};
extern struct task_group *task_groups;

typedef int (*dc_plugin_init)();
typedef int (*dc_plugin_teardown)();
typedef int (*dc_plugin_register)         (struct task *task);
typedef int (*dc_plugin_unregister)       (struct task *task);
typedef int (*dc_plugin_prepare)          (struct task *task);
typedef int (*dc_plugin_prepare_child)    (struct task *task);
typedef int (*dc_plugin_prepare_parent)   (struct task *task);
typedef int (*dc_plugin_exec)             (struct task *task);
typedef int (*dc_plugin_terminate)        (struct task *task);
typedef int (*dc_plugin_select)           (fd_set *, int *maxfd);
typedef int (*dc_plugin_activate)         (fd_set *);
typedef int (*dc_plugin_register_group)   (struct task_group *group);
typedef int (*dc_plugin_unregister_group) (struct task_group *group);

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
    dc_plugin_register_group reg_group;
    dc_plugin_unregister_group unreg_group;

    struct dc_plugin *next;
};
