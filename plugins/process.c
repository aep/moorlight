#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>

#include "plugin.h"
#include "logger.h"


static int sigp[2];

static void sigterm()  { write(sigp[1], "T", 1); }
static void sigchild() { write(sigp[1], "C", 1); }



#define KCLOSE(p) if (p != -1) close (p); p = -1;

struct process
{
    int startp[2];
    int deathp[2];
};

int process_init()
{
    assume(pipe(sigp));
    fcntl(sigp[0],F_SETFL,fcntl(sigp[0],F_GETFL)|O_NONBLOCK|FD_CLOEXEC);
    fcntl(sigp[1],F_SETFL,fcntl(sigp[1],F_GETFL)|O_NONBLOCK|FD_CLOEXEC);

    signal(SIGINT,   sigterm);
    signal(SIGTERM,  sigterm);
    signal(SIGILL,   sigterm);
    //signal(SIGSEGV,  sigterm);
    signal(SIGCHLD,  sigchild);

    log_debug("process", "plugin initialized");
    return 0;
}

int process_teardown()
{
    return 0;
}

int process_register_group(struct task_group *group)
{
    return 0;
}

int process_unregister_group(struct task_group *group)
{
    return 0;
}

int process_register(struct task *task)
{
    struct process *proc = calloc(1, sizeof(struct process));
    task->pp_proc = proc;

    return 0;
}

int process_unregister(struct task *task)
{
    free(task->pp_proc);
    task->pp_proc = 0;
    return 0;
}

int process_prepare(struct task *task)
{
    struct process *proc = task->pp_proc;

    assume(pipe(proc->startp));
    fcntl(proc->startp[0],F_SETFL,fcntl(proc->startp[0],F_GETFL)|O_NONBLOCK);
    fcntl(proc->startp[1],F_SETFL,fcntl(proc->startp[1],F_GETFL)|O_NONBLOCK);
    assume(pipe(proc->deathp));
    fcntl(proc->deathp[0],F_SETFL,fcntl(proc->deathp[0],F_GETFL)|O_NONBLOCK);
    fcntl(proc->deathp[1],F_SETFL,fcntl(proc->deathp[1],F_GETFL)|O_NONBLOCK);
    return 0;
}

int process_prepare_child(struct task *task)
{
    struct process *proc = task->pp_proc;
    KCLOSE(proc->startp[0]);
    fcntl(proc->startp[1], F_SETFD, fcntl(proc->startp[1], F_GETFD) | FD_CLOEXEC);

    // become session leader
    setsid();
    return 0;
}

int process_prepare_parent(struct task *task)
{
    struct process *proc = task->pp_proc;

    KCLOSE(proc->startp[1]);

    log_debug("process", "[%s] has pid %i", task->name, task->pid);
    char buff;

    int ret;
    for (;;) {
        ret = read(proc->startp[0], &buff, 1);
        if (ret == 0) {
            log_debug("process", "[%s] startpipe sycned. all good.", task->name);
            return 0;
        } else  if (ret < 1) {
            if (errno == EAGAIN)
                continue;
            else if (ret == 0)
                assume(ret);
        } else {
            log_error("process", "[%s] startpipe indicates error %i: %s. ",  task->name, buff, strerror(buff));
            return buff;
        }
    }
    return 0;
}

int process_stop(struct task *task)
{
    struct process *proc = task->pp_proc;
    KCLOSE(proc->startp[0]);

    if (!task->pid) {
        log_error("process", "[%s] has pid %i", task->name, task->pid);
        return 2;
    }
    log_debug("process", "[%s] terminating session %i", task->name, task->pid);

    //nuke the entire process group
    kill(-task->pid, SIGTERM);

    //TODO: wait for all of them do die or is waiting for the leader sufficient?
    //TODO: add real wait mechanism with timeouts
    int status;

    log_debug("process", "[%s] waiting for leader %i to die", task->name, task->pid);
    waitpid(task->pid, &status, 0);
    log_debug("process", "[%s] leader %i finally died", task->name, task->pid);

    return 0;
}

int process_exec(struct task *task)
{
    struct process *proc = task->pp_proc;
    log_debug("process", "about to exec %s", task->cmd);
    // execute

    //TODO: this is just a hack
    char *argv [1000];
    int i = 0;
    char *cop = strdup(task->cmd);
    char *tk = strtok(cop, " \t");
    while (tk) {
        argv[i++] = tk;
        tk = strtok(NULL,  " \t");
    }
    argv[i] = 0;
    execv(*argv, argv);

    //ok, that didn't work.
    write(proc->startp[1], (char*)&errno, 1);
    return 666;
}

int process_select (fd_set *rfds, int *maxfd)
{
    FD_SET(sigp[0], rfds);
    if (sigp[0] > *maxfd)
        *maxfd = sigp[0];
    return 0;
}

int dc_restart(struct task *task);
static void update()
{
    for (struct task_group *i = task_groups; i ; i = i->next) {
        for (struct task *task = i->tasks; task ; task = task->next) {
            if (task->running != 0) {
                if (waitpid(task->pid, &(task->state), WNOHANG)) {
                    if (WIFEXITED(task->state) || WIFSIGNALED(task->state) ) {
                        log_info("process", "[%s] leader died", task->name);
                        dc_on_died(task);
                    }
                }
            }
        }
    }
    // TODO: detect processes that double forked and are now our child
}

int process_activate(fd_set *rfds)
{
    char buf [10];
    if (FD_ISSET(sigp[0], rfds)) {
        buf[0] = 'D';
        read(sigp[0], buf, 1);
        switch (buf[0]) {
            case 'D':
                log_error("process", "sigp (%i) is dead. wtf? assuming glitch and trying to go on", sigp[0]);
                sleep (6);
                break;;
            case 'C':
                update();
                break;;
            case 'T':
                log_info("process", "init got a sigterm. tearing down the whole thing.");
                dc_quit();
                break;;
            default:
                log_debug("process", "sigp says '%c'. *shrug* ",  buf[0]);
        }
    }
    return 0;
}

#ifdef DYNAMIC_PLUGINS
struct dc_plugin process_plugin =
{
    "process",
    &process_init,
    &process_teardown,
    &process_register,
    &process_unregister,
    &process_prepare,
    &process_prepare_child,
    &process_prepare_parent,
    &process_exec,
    &process_stop,
    &process_select,
    &process_activate,
    &process_register_group,
    &process_unregister_group
    0
};
#endif

