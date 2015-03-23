CC=gcc
CFLAGS=-c -Wall -D_GNU_SOURCE -ggdb
LDFLAGS=-lirc -lcrypto -lssl -ldl -pthread
TARGET_SOURCES=framework.c
TARGET_OBJECTS=$(TARGET_SOURCES:.c=.o)
EXECUTABLE=mainframe
API_DIR=api
PLUGIN_DIR=plugins

.PHONY: all target api plugins

all: target api plugins

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
