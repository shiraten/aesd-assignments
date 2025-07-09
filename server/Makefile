.PHONY: all default

default: aesdsocket
all: aesdsocket

CFLAGS += -g -Wall -Werror

aesdsocket: aesdsocket.c
	$(CROSS_COMPILE)gcc ${CFLAGS} -o aesdsocket aesdsocket.c

clean :
	rm -rf aesdsocket *.o
