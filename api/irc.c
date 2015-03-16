#include "irc.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <openssl/ssl.h>

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


ssize_t irc_recv_all(bot_t *bot, char **bufptr, size_t *n)
{
    ssize_t bytes_read = 0;
    size_t received = 0;

    if (*bufptr == NULL) {
        *bufptr = (char *) malloc(1025);
        *n = 1025;
    }

    while (1) {
        if (*n - bytes_read <= 1024) {
            *bufptr = realloc(*bufptr, *n + 1024);
            *n += 1024;
        }

        received = irc_recv(bot, *bufptr + bytes_read, 1024, 0);
        bytes_read += received;

        if (received < 1024)
            break;
    }

    (*bufptr)[bytes_read] = '\0';
    return bytes_read;
}


ssize_t irc_recv_flush_to_fp(bot_t *bot, FILE *stream)
{
    char *buf = NULL;
    ssize_t len;
    size_t n;

    len = irc_recv_all(bot, &buf, &n);
    fprintf(stream, "%s", buf);
    free(buf);

    return len;
}


int irc_join(bot_t *bot, char *channel)
{
    char *msg;

    asprintf(&msg, "JOIN %s\n", channel);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_part(bot_t *bot, char *channel)
{
    char *msg;

    asprintf(&msg, "PART %s\n", channel);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 1;
}


int irc_nick(bot_t *bot, char *nick)
{
    char *msg;

    asprintf(&msg, "NICK %s\n", nick);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 0;
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
