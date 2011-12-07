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
#include "main.h"

#define MOD_PROCESS 1
#define MOD_UBUS 1
// #define MOD_SYSV 1

#if defined(MOD_PROCESS)
#define mod_process(f) mod_process_ ## f
#else
#define mod_process(f) 0
#endif

#if defined(MOD_UBUS)
#define mod_ubus(f) mod_ubus_ ## f
#else
#define mod_ubus(f) 0
#endif

#if defined(MOD_SYSV)
#define mode_sysv(f) mod_sysv_ ## f
#else
#define mod_sysv(f) 0
#endif

/////-------------------------------- events ------------------------------------------

int dc_on_task_started(struct task *task)
{
    mod_ubus(on_task_started(task));
    return 0;
}

int dc_on_task_stopped(struct task *task)
{
    mod_ubus(on_task_stopped(task));
    return 0;
}

int dc_on_task_died(struct task *task)
{
    time_t now = time(0);
    if (now - task->last_died < 5) {
        if (task->toofastcounter++ > 10) {
            log_error("dc", "task %s respawing too fast. human intervention required", task->name);
            dc_task_stop(task);
            return 3;
        }
    }
    task->toofastcounter = 0;
    task->last_died = now;
    dc_task_restart(task);
    return 0;
}

/////-------------------------------- hooks ------------------------------------------

int dc_fork_child(struct task *task)
{
    return 0;
}

int dc_fork_parent(struct task *task)
{
    return 0;
}

/////-------------------------------- control ------------------------------------------

int dc_task_start(struct task *task)
{
    if (task->running != 0)
        return 0;
    task->running = 1;
    int r = 0;
    r =  mod_process(task_start(task));
    if (r != 0) {
        log_error("dc", "task starting failed for %s", task->name);
        task->running = 4;
        dc_task_stop(r);
        return r;
    }

    dc_on_task_started(task);
    return r;
}

int dc_task_stop(struct task *task)
{
    int r = 0;
    if (task->running == 0 )
        return 0;

    mod_process(task_stop(task));
    task->running = 0;

    dc_on_task_stopped(task);
    return 0;
}

int dc_task_restart(struct task *task)
{
    dc_task_stop(task);
    dc_task_start(task);
    return 0;
}

/////-------------------------------- registry ------------------------------------------

struct task_group *task_groups = 0;

void dc_delete_group(struct task_group *group);
struct task_group *dc_new_group(const char *name)
{
    struct task_group *group = calloc(1,  sizeof(struct task_group));
    group->name = strdup(name);

    if (task_groups) {
        assert (task_groups->prev == 0);
        task_groups->prev = group;
        group->next = task_groups;
    }
    task_groups = group;

    int r = 0;
    r = mod_ubus(new_group(group)); if (r != 0) goto unroll;

    return group;
unroll:
    dc_delete_group(group);
    return 0;
}
void dc_delete_group(struct task_group *group)
{
    mod_ubus(delete_group(group));

    if (group->prev)
        group->prev->next = group->next;
    if (group->next)
        group->next->prev = group->prev;
    if (task_groups == group)
        task_groups = group->next;

    free (group->name);
    free (group);
}



void dc_delete_task(struct task *task);
struct task *dc_new_task(struct task_group *group, const char *name)
{
    struct task *task = calloc(1,  sizeof(struct task));
    for (struct task_group *i = task_groups; i ; i = i->next) {
        for (struct task *j = i->tasks; j ; j = j->next) {
            if (strcmp(task->name, name) == 0) {
                log_error("dc", "task %s already registered in group %s", name, i->name);
                return 0;
            }
        }
    }
    task->group = group;
    task->name = strdup(name);
    if (group->tasks) {
        assert (group->tasks->prev == 0);
        group->tasks->prev = task;
        task->next = group->tasks;
    }
    group->tasks = task;

    if (mod_process  (new_task(task)) != 0) goto unroll;
    if (mod_ubus     (new_task(task)) != 0) goto unroll;

    return task;

unroll:
    dc_delete_task(task);
    return 0;
}
void dc_delete_task(struct task *task)
{

    mod_ubus    (delete_task(task));
    mod_process (delete_task(task));


    if (task->prev)
        task->prev->next = task->next;
    if (task->next)
        task->next->prev = task->prev;
    if (task->group->tasks == task)
        task->group->tasks = task->next;

    if (task->name)
        free (task->name);
    if (task->cmd)
        free (task->cmd);
    free (task);
}



/////-------------------------------- mainloop ------------------------------------------

static int running = 1;
int dc_quit()
{
    running = 0;
}

int main(int argc, char **argv)
{
    if (getuid() != 0)
        panic("sorry, must be superuser");

    int iaminit = 0;
    if (strcmp(argv[0], "/sbin/init") == 0) {
        iaminit = 1;
        //FIXME: ugh
        system("mount -n tmpfs -t tmpfs /task");
    }

    log_info("dc", "Moorlight 1 booting up");

    int r = 0;
    if (iaminit) r = mod_sysv (init());
    r = mod_process (init());
    r = mod_ubus    (init());

    struct task_group *default_group = dc_new_group("default");
    struct task *test = dc_new_task(default_group, "test");
    test->cmd  = strdup("/home/aep/proj/moorlight/test/testdaemon.sh");

    for (struct task_group *i = task_groups; i ; i = i->next)
        for (struct task *j = i->tasks; j ; j = j->next)
            dc_task_start(j);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = 0;
        mod_ubus     (select(&rfds, &maxfd));
        mod_process  (select(&rfds, &maxfd));

        int ret = 0;
        do {
            int ret = select(maxfd + 2, &rfds, NULL, NULL, NULL);
        } while (ret == EINTR);

        if (ret < 0) {
            assume(ret);
        }

        log_debug("dc", "activated %i ", ret);

        mod_ubus    (activate(&rfds));
        mod_process (activate(&rfds));
    }

    for (struct task_group *i = task_groups; i ; i = i->next) {
        for (struct task *j = i->tasks; j ; j = j->next) {
            dc_task_stop(j);
            dc_delete_task(j);
        }
        int r = 0;
        dc_delete_group(i);
    }

    mod_ubus     (teardown());
    mod_process  (teardown());

    if (iaminit)
        mod_sysv (teardown());


    return 0;
}


