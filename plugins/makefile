CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb -fpic
LDFLAGS=-lirc -shared
LIB_SOURCES=rps.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=librps.so

.PHONY: all plugin

all: plugin

plugin: $(LIB)

$(LIB): $(LIB_OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)
	sudo cp $@ /usr/lib/

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o
