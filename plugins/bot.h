#ifndef _bot_struct_h
#define _bot_struct_h

#include "list.h"
#include <stdint.h>
#include <openssl/ssl.h>

typedef struct plugin {
    struct list_head list;

    char *name;
    char *path;
    void *handle;
    int (*init_func)();
    int (*handle_func)(char *,char *, char *);
    int (*fini_func)();
} plugin_t;

typedef struct channel {
    struct list_head list;

    char *name;
    char *topic;
    char *nicks;
} channel_t;

typedef struct bot {
    struct list_head list;

    char *name;
    char *server;
    char *port;
    uint8_t ssl;

    char *user;
    char *nick;
    char *pass;
    char *maintainer;

    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;

    struct list_head channels;
    struct list_head plugins;
} bot_t;

#endif
