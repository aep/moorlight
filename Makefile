CFLAGS=-g -I. -std=c99 -D_GNU_SOURCE -Wno-implicit-function-declaration
LFLAGS=-lubus

bin/moorlight:main.c logger.c modules/cgroup.c modules/process.c modules/sysv.c modules/ubus.c
	mkdir -p bin; gcc ${CFLAGS} $^ -o $@ ${LFLAGS}

clean:
	rm -rf bin
