#include <openssl/ssl.h>

typedef struct {
    int sock;
    SSL *sslHandle;
    SSL_CTX *sslContext;
} Connection;

Connection * irc_connect(char *hostname, char *port, int ssl, char *nick,
                         char *nickpass, char *user, char *realname);

int irc_join(Connection *conn, char *channel);

int irc_part(Connection *conn, char *channel);

int irc_nick(Connection *conn, char *nick);

int irc_msg(Connection *conn, char *rcpt, char *msg);
