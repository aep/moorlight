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
    int running;
    time_t last_died;
    int toofastcounter;

    void *pp_proc;

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
