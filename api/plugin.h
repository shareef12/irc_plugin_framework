#ifndef _irc_h
#define _irc_h

#include <stdarg.h>

/**
 * irc_send - Low level sending function. Plugins should prefer higher level
 *            functions listed below.
 */
ssize_t bot_send(char *buf, size_t len, int flags);

/**
 * irc_msg - Send an irc message to a channel or user.
 * @rcpt:   Recipient of the message
 * @fmt:    Format string for the message. Similar calling convention as printf
 */
int bot_msg(char *rcpt, char *fmt, ...);

/**
 * bot_get_server - Get the server the bot is currently connected to.
 */
char * bot_get_server();

/**
 * bot_get_channels - Get a NULL terminated list of all channels the bot is currently in.
 */
char ** bot_get_channels();

/**
 * bot_get_nick - Get the bot's current nick.
 */
char * bot_get_nick();

/**
 * bot_get_nicks - Get a NULL terminated list of nicks from a specified channel.
 * @channel: channel to get the nicks from (bot must be joined)
 */
char ** bot_get_nicks(char *channel_name);

/**
 * bot_get_topic - Get the topic of a specified channel.
 * @channel - The channel to get the topic from (bot must be joined)
 */
char * bot_get_topic(char *channel_name);

#endif
