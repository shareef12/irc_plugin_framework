CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb -fpic
LDFLAGS=-lcrypto -lssl -shared
LIB_SOURCES=irc.c
LIB_OBJECTS=$(LIB_SOURCES:.c=.o)
LIB=libirc.so

.PHONY: all api

all: api

api: $(LIB)

$(LIB): $(LIB_OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)
	sudo cp $@ /usr/lib/

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o
