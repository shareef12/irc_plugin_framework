CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb
LDFLAGS=-lirc -ldl
TARGET_SOURCES=bot.c
TARGET_OBJECTS=$(TARGET_SOURCES:.c=.o)
EXECUTABLE=cbot
API_DIR=api
PLUGIN_DIR=plugins

.PHONY: all api plugins target

all: api plugins target

api:
	$(MAKE) -C $(API_DIR)
plugins:
	$(MAKE) -C $(PLUGIN_DIR)
target: $(EXECUTABLE)

$(EXECUTABLE): $(TARGET_OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $< -o $@

clean:
	$(RM) *.o
	$(MAKE) -C $(API_DIR) clean
	$(MAKE) -C $(PLUGIN_DIR) clean
