#ifndef _bot_h
#define _bot_h

#include "list.h"
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
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
    char **nicks;
    unsigned int nNicks;
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
    
    pthread_t thread;
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
    
    unsigned int nChannels;
    struct list_head channels;
    struct list_head plugins;
} bot_t;

extern struct list_head bots;

ssize_t irc_send(bot_t *bot, char *buf, size_t len, int flags);

int irc_msg(bot_t *bot, char *rcpt, char *msg);

ssize_t irc_recv(bot_t *bot, void *buf, size_t len, int flags);

ssize_t irc_recv_flush_to_fp(bot_t *bot, FILE *stream);

// Must be called from a bot running in an independent thread
ssize_t bot_send(char *buf, size_t len, int flags);

#endif
