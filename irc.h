#include <openssl/ssl.h>

typedef struct {
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} Connection;

int irc_connect(char *hostname, char *port, int ssl, char *nick,
                char *nickpass, char *user, char *realname);

void irc_disconnect();

ssize_t irc_send(char *buf, size_t len, int flags);

ssize_t irc_recv(void *buf, size_t len, int flags);

ssize_t irc_recv_all(char **bufptr, size_t *n);

ssize_t irc_recv_flush_to_fp(FILE *stream);

int irc_join(char *channel);

int irc_part(char *channel);

int irc_nick(char *nick);

int irc_user(char *user, char *mode, char *realname);

int irc_msg(char *rcpt, char *msg);

int irc_pong(char *ping);
