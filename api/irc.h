#ifndef _irc_h
#define _irc_h

#include "../bot.h"
#include <stdarg.h>
#include <stdint.h>

ssize_t irc_send(bot_t *bot, char *buf, size_t len, int flags);

ssize_t irc_recv(bot_t *bot, void *buf, size_t len, int flags);

ssize_t irc_recv_all(bot_t *bot, char **bufptr, size_t *n);

ssize_t irc_recv_flush_to_fp(bot_t *bot, FILE *stream);

int irc_join(bot_t *bot, char *channel);

int irc_part(bot_t *bot, char *channel);

int irc_nick(bot_t *bot, char *nick);

int irc_msg(bot_t *bot, char *rcpt, char *fmt, ...);

#endif
