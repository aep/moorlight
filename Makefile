CFLAGS=-g -I.
LFLAGS=-lubus

moorlight:main.c logger.c plugins/cgroup.c plugins/process.c plugins/sysv.c plugins/ubus.c
	gcc ${CFLAGS} $^ -o $@ ${LFLAGS}

clean:
	rm -f moorlight
