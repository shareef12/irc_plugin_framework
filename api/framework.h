#ifndef _bot_h
#define _bot_h

#include "list.h"
#include <stdint.h>
#include <stdarg.h>
#include <pthread.h>
#include <openssl/ssl.h>

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

/**
 * bot_create - Create a bot with no connections or plugins
 * @name:   The generic name for the bot (Not IRC nick)
 * @server: IP address or hostname of IRC server
 * @port:   IRC server port to connect to
 * @SSL:    Flag to use SSL (1 if ssl, 0 otherwise)
 * @user:   Username for the bot
 * @nick:   Nick for the bot
 * @pass:   Password for the bot's nick (used in IDENTIFY with nickserv)
 * @maintainer: Nick of the bot maintainer
 */ 
bot_t * bot_create(char *name, char *server, char *port, uint8_t ssl,
               char *user, char *nick, char *pass, char *maintainer);

/**
 * bot_destroy - Destroy and free all resources owned by a bot
 * @bot:    The bot to destroy
 * @reason: The reason the bot is being destroyed (sent as QUIT msg to IRC server)
 */
void bot_destroy(char *bot_name, char *reason);

void bot_list_plugins(char *bot_name);

/**
 * bot_add_plugin - Find, load, and assign a .so plugin to a bot
 * @bot:    The bot that will use the plugin
 * @path:   Path to the .so file to use for the plugin (man dlopen for more
 *          information on how the path is expanded
 */
int bot_add_plugin(char *bot_name, char *filename);

/**
 * bot_remove_plugin - Unloads the specified plugin
 * @bot:    The bot to remove the plugin from
 * @identifier: The path (preferred) or name of the plugin to unload
 */
int bot_remove_plugin(char *bot_name, char *filename);

void bot_list_channels(char *bot_name);

int bot_join_channel(char *bot_name, char *channel);

int bot_part_channel(char *bot_name, char *channel, char *reason);

int bot_change_nick(char *bot_name, char *nick);

void list_bot_names();

// Must be called from a bot running in an independent thread
ssize_t bot_send(char *buf, size_t len, int flags);

// Must be called from a bot running in an independent thread
char * bot_recv_all();

#endif
