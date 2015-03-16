#ifndef _bot_h
#define _bot_h

#include "list.h"
#include <stdint.h>
#include <openssl/ssl.h>

#include "plugins/bot.h"

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
void bot_destroy(bot_t *bot, char *reason);

/**
 * bot_add_plugin - Find, load, and assign a .so plugin to a bot
 * @bot:    The bot that will use the plugin
 * @path:   Path to the .so file to use for the plugin (man dlopen for more
 *          information on how the path is expanded
 */
int bot_add_plugin(bot_t *bot, char *path);

/**
 * bot_remove_plugin - Unloads the specified plugin
 * @bot:    The bot to remove the plugin from
 * @identifier: The path (preferred) or name of the plugin to unload
 */
int bot_remove_plugin(bot_t *bot, char *identifier);

#endif
