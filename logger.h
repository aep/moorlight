#include <errno.h>

void log_info (const char *component, const char *msg, ...);
void log_debug(const char *component, const char *msg, ...);
void log_error(const char *component, const char *msg, ...);
void panic(const char *msg, ... );


#define assume(call) { if ((call) < 0) { panic("============================================\n"\
"PANIC in %s:%i -> %s\nsyscall %s failed with error %i:%s", __FILE__, __LINE__, __PRETTY_FUNCTION__, #call \
, errno, strerror(errno)); }}
