//#include "bot.h"

#include "list.h"

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <netdb.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
#include <openssl/err.h>


/**
 * TODO: Redesign nick storage
 * TODO: Implement bot_get_nicks
 */

struct list_head bots;

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


static bot_t * get_bot_by_thread()
{
    bot_t *bot;
    pthread_t thread = pthread_self();

    list_for_each_entry(bot, &bots, list) {
        if (bot->thread == thread)
            return bot;
    }

    return NULL;
}


channel_t * get_channel_by_name(struct list_head *channels, char *channel_name)
{
    channel_t *channel;

    list_for_each_entry(channel, channels, list) {
        if (strcmp(channel_name, channel->name) == 0)
            return channel;
    }

    return NULL;
}

 
ssize_t irc_recv(bot_t *bot, void *buf, size_t len, int flags)
{
    ssize_t bytesRecv;
    if (bot->sslHandle == NULL) {
        bytesRecv = recv(bot->socket, buf, len, flags);
    }
    else {
        bytesRecv = SSL_read(bot->sslHandle, buf, len);
    }

    return bytesRecv;
}


ssize_t irc_recv_flush_to_fp(bot_t *bot, FILE *stream)
{
    char *buf = malloc(1500);
    ssize_t len = 0;
    struct timeval timeout = {2, 0};

    setsockopt(bot->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    len = irc_recv(bot, buf, 1500, 0);
    if (len > 0) {
        buf[len] = 0;
        fprintf(stream, "%s", buf);
        free(buf);
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(bot->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    return len;
}


ssize_t irc_send(bot_t *bot, char *buf, size_t len, int flags)
{
    ssize_t bytesSent;
    
    if (bot->sslHandle == NULL) {
        bytesSent = send(bot->socket, buf, len, flags);
    }
    else {
        bytesSent = SSL_write(bot->sslHandle, buf, len);
    }

    return bytesSent;
}


int irc_msg(bot_t *bot, char *rcpt, char *fmt, ...)
{
    char *buf, *msg;
    va_list ap;
    
    va_start(ap, fmt);
    asprintf(&buf, "PRIVMSG %s :%s\n", rcpt, fmt);
    vasprintf(&msg, buf, ap);
    va_end(ap);

    irc_send(bot, msg, strlen(msg), 0);

    free(buf);
    free(msg);
    return 0;
}


ssize_t bot_send(char *buf, size_t len, int flags)
{
    bot_t *bot;
    
    bot = get_bot_by_thread();
    if (bot == NULL)
        return -1;
    
    return irc_send(bot, buf, len, flags);
}


int bot_msg(char *rcpt, char *fmt, ...)
{
    char *buf, *msg;
    va_list ap;
    
    va_start(ap, fmt);
    asprintf(&buf, "PRIVMSG %s :%s\n", rcpt, fmt);
    vasprintf(&msg, buf, ap);
    va_end(ap);

    bot_send(msg, strlen(msg), 0);

    free(buf);
    free(msg);
    return 0;
}


char * bot_get_server()
{
    bot_t *bot;
    char *buf;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    asprintf(&buf, "%s:%s", bot->server, bot->port);
    return buf;
}


char ** bot_get_channels()
{
    bot_t *bot;
    channel_t *channel;
    char **array;
    int i = 0;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    array = malloc((bot->nChannels + 1) * sizeof(char *));
    list_for_each_entry(channel, &bot->channels, list)
        array[i++] = strdup(channel->name);

    array[i] = NULL;
    return array;
}


char * bot_get_nick()
{
    bot_t *bot;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    return strdup(bot->nick);
}


char ** bot_get_nicks(char *channel_name)
{
    bot_t *bot;
    channel_t *channel;
    
    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    channel = get_channel_by_name(&bot->channels, channel_name);

    return channel->nicks;
}


char * bot_get_topic(char *channel_name)
{
    bot_t *bot;
    channel_t *channel;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }

    channel = get_channel_by_name(&bot->channels, channel_name);
    if (channel == NULL)
        return NULL;

    return channel->topic;
}
