CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb -fpic
LDFLAGS=-lirc -shared
LIB_SOURCES=plugin.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=libmsg.so

.PHONY: all plugin

all: plugin

plugin: $(LIB)

$(LIB): $(LIB_OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o
