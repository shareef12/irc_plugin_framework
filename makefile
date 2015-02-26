CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb
#COMMON_SOURCES=irc.c
#TARGET_SOURCES=bot.c
#COMMON_OBJECTS=$(COMMON_SOURCES:.c=.o)
#TARGET_OBJECTS=$(TARGET_SOURCES:.c=.o)
#EXECUTABLE=multiBot

all: libirc.so cbot

libirc.so: irc.o
	$(CC) $< -o $@ -lcrypto -lssl -shared
	sudo cp $@ /usr/lib/

irc.o: irc.c
	$(CC) $(CFLAGS) $< -o $@ -fpic

cbot: bot.o
	$(CC) $< -o $@ -lirc -ldl

bot.o: bot.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o
