CC ?= gcc
CFLAGS = -Wall -Wextra -g

# Set cross-compiler if CROSS_COMPILE is defined
ifeq ($(CROSS_COMPILE),)
    CROSS_CC = $(CC)
else
	CROSS_CC = $(CROSS_COMPILE)gcc
endif


all: aesdsocket

aesdsocket: aesdsocket.o
	$(CROSS_CC) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CROSS_CC) $(CFLAGS) -c aesdsocket.c
	
clean:
	rm -f aesdsocket aesdsocket.o /var/tmp/aesdsocketdata
