CFLAGS=-g -I. -std=c99 -D_GNU_SOURCE -Wno-implicit-function-declaration
LFLAGS=-lubus

moorlight:main.c logger.c plugins/cgroup.c plugins/process.c plugins/sysv.c plugins/ubus.c
	gcc ${CFLAGS} $^ -o $@ ${LFLAGS}

clean:
	rm -f moorlight
