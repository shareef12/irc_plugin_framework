CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb
LDFLAGS=-lcrypto -lssl -ldl
COMMON_SOURCES=irc.c
TARGET_SOURCES=bot.c
COMMON_OBJECTS=$(COMMON_SOURCES:.c=.o)
TARGET_OBJECTS=$(TARGET_SOURCES:.c=.o)
EXECUTABLE=multiBot

all: target

target: $(EXECUTABLE)

$(EXECUTABLE): $(COMMON_OBJECTS) $(TARGET_OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o

