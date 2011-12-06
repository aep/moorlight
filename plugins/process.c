#include <fcntl.h>
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

int process_init()
{
    assume(pipe(sigp));
    fcntl(sigp[0],F_SETFL,fcntl(sigp[0],F_GETFL)|O_NONBLOCK);
    fcntl(sigp[1],F_SETFL,fcntl(sigp[1],F_GETFL)|O_NONBLOCK);

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
    return 0;
}

int process_unregister(struct task *task)
{
    return 0;
}

int process_prepare(struct task *task)
{
    assume(pipe(task->proccom));
    fcntl(task->proccom[0],F_SETFL,fcntl(task->proccom[0],F_GETFL)|O_NONBLOCK);
    fcntl(task->proccom[1],F_SETFL,fcntl(task->proccom[1],F_GETFL)|O_NONBLOCK);
    return 0;
}

int process_prepare_child(struct task *task)
{
    // become session leader
    setsid();

    return 0;
}

int process_prepare_parent(struct task *task)
{
    close(task->proccom[1]);
    log_debug("process", "[%s] has pid %i", task->name, task->pid);
    char buff;

    int ret;
    for (;;) {
        ret = read(task->proccom[0], &buff, 1);
        if (ret < 1) {
            if (errno == EAGAIN)
                continue;
            else
                assume(ret);
        } else {
            break;
        }
    }
    log_debug("process", "[%s] pipe sycned %i", task->name, ret);
    return 0;
}

int process_stop(struct task *task)
{
    if (!task->pid)
        return 2;
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
    close(task->proccom[0]);
    write(task->proccom[1], "S", 1);
    execv(*argv, argv);
    return 0;
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
                        dc_restart(task);
                    }
                }
            }
        }
    }
    // TODO: detect processes that double forked and are now our child
}

int process_activate(fd_set *rfds)
{
    if (FD_ISSET(sigp[0], rfds)) {
        char buf [10];
        assume(read(sigp[0], buf, 1));
        switch (buf[0]) {
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

