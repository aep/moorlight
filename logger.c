#include "logger.h"

#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>


void log_info(const char *component, const char *msg, ...)
{
    fprintf(stderr, "[INFO]  [%s] ", component);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void log_debug(const char *component, const char *msg, ...)
{
    fprintf(stderr, "[DEBUG] [%s] ", component);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
}

void log_error(const char *component, const char *msg, ...)
{
    fprintf(stderr, "\033[0;31m[ERROR] [%s] ", component);
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\033[0m\n");
}


void panic(const char *msg, ... )
{
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(errno);
}
