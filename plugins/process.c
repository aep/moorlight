#include "plugin.h"
#include "logger.h"

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

int process_prepare(struct dc *dc)
{
    dc->running = 1;
    assume(pipe(dc->proccom));
    fcntl(dc->proccom[0],F_SETFL,fcntl(dc->proccom[0],F_GETFL)|O_NONBLOCK);
    fcntl(dc->proccom[1],F_SETFL,fcntl(dc->proccom[1],F_GETFL)|O_NONBLOCK);
    return 0;
}

int process_prepare_child(struct dc *dc)
{
    // become session leader
    setsid();

    close(dc->proccom[0]);
    write(dc->proccom[1], "S", 1);
    return 0;
}

int process_prepare_parent(struct dc *dc)
{
    close(dc->proccom[1]);
    log_debug("process", "[%s] has pid %i", dc->name, dc->pid);
    char buff;
    int ret = read(dc->proccom[0], &buff, 1);
    log_debug("process", "[%s] pipe sycned %i", dc->name, ret);
    return 0;
}

int process_terminate(struct dc *dc)
{
    log_debug("process", "[%s] terminating session %i", dc->name, dc->pid);

    //nuke the entire process group
    kill(-dc->pid, SIGTERM);

    //TODO: wait for all of them do die or is waiting for the leader sufficient?
    int status;
    waitpid(dc->pid, &status, 0);

    return 0;
}

int process_exec(struct dc *dc)
{
    log_debug("process", "about to exec %s", dc->cmd);
    // execute

    //TODO: this is just a hack
    char *argv [1000];
    int i = 0;
    char *cop = strdup(dc->cmd);
    char *tk = strtok(cop, " \t");
    while (tk) {
        argv[i++] = tk;
        tk = strtok(NULL,  " \t");
    }
    argv[i] = 0;
    execv(*argv, argv);
    return 0;
}

int process_select (dc_list *dc, fd_set *rfds, int *maxfd)
{
    FD_SET(sigp[0], rfds);
    if (sigp[0] > *maxfd)
        *maxfd = sigp[0];
    return 0;
}

int dc_restart(struct dc *dc);
static void update(dc_list *dci)
{
    while (dci) {
        struct dc *dc = dci;
        if (waitpid(dc->pid, &(dc->state), WNOHANG)) {
            if (WIFEXITED(dc->state) || WIFSIGNALED(dc->state) ) {
                log_info("process", "[%s] leader died", dc->name);
                dc->running = 2;
                dc_restart(dc);
            }
        }
        dci = dci->next;
    }
}

int process_activate(dc_list *dc, fd_set *rfds)
{
    if (FD_ISSET(sigp[0], rfds)) {
        char buf [10];
        assume(read(sigp[0], buf, 1));
        switch (buf[0]) {
            case 'C':
                update(dc);
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
    &process_prepare,
    &process_prepare_child,
    &process_prepare_parent,
    &process_exec,
    &process_terminate,
    &process_select,
    &process_activate,
    0
};
#endif

