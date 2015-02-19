#include "irc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/rand.h>
#include <openssl/err.h>

#define  SERVER     "beitshlomo.com"
#define  PORT       "6697"
#define  SSL        1
#define  NICK       "cbot"
#define  NICKPASS   "holaa"
#define  USER       "shareef12"
#define  REALNAME   "shareef12"


//TODO: Close gracefully on SIGSEG/SIGINT/SIGTERM
//TODO: Check for memory leaks
//TODO: Develop plugin architecture


static ssize_t irc_send(Connection *conn, char *buf, size_t len, int flags)
{
    ssize_t bytesSent;
    if (conn->sslHandle == NULL) {
        bytesSent = send(conn->sock, buf, len, flags);
    }
    else {
        bytesSent = SSL_write(conn->sslHandle, buf, len);
    }

    return bytesSent;
}


static ssize_t irc_recv(Connection *conn, void *buf, size_t len, int flags)
{
    ssize_t bytesRecv;
    if (conn->sslHandle == NULL) {
        bytesRecv = recv(conn->sock, buf, len, flags);
    }
    else {
        bytesRecv = SSL_read(conn->sslHandle, buf, len);
    }

    return bytesRecv;
}


/*
  irc_recv_all is similar to the getline function. Read the man page for 
  getline to understand this function's behavior.
*/
static ssize_t irc_recv_all(Connection *conn, char **bufptr, size_t *n)
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

        received = irc_recv(conn, *bufptr + bytes_read, 1024, 0);
        bytes_read += received;

        if (received < 1024)
            break;
    }

    (*bufptr)[bytes_read] = '\0';
    return bytes_read;
}


static ssize_t irc_recv_flush_to_fp(Connection *conn, FILE *stream)
{
    char *buf = NULL;
    ssize_t len;
    size_t n;

    len = irc_recv_all(conn, &buf, &n);
    fprintf(stream, "%s", buf);
    free(buf);

    return len;
}


static int irc_pong(Connection *conn, char *ping)
{
    ping[1] = 'O';
    irc_send(conn, ping, strlen(ping), 0);

    return 0;
}


Connection * irc_connect(char *hostname, char *port, int ssl, char *nick,
                         char *nickpass, char *user, char *realname)
{
    int s, ping = 0;
    struct addrinfo hints, *result, *rp;
    Connection *conn= (Connection *) malloc(sizeof(Connection));
    char *buf = NULL;
    size_t n;
 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    s = getaddrinfo(hostname, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    for (rp = result ; rp != NULL; rp = rp->ai_next) {
        conn->sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (conn->sock == -1)
            continue;
        
        if (connect(conn->sock, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(conn->sock);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not conn->ct to %s:%s\n", hostname, port);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);
    printf("Connected to %s:%s. Socket fd: %d\n", hostname, port, conn->sock);

    conn->sslHandle = NULL;
    conn->sslContext = NULL;
    if (ssl) {
        printf("Beginning SSL Negotiation...\n");
        SSL_load_error_strings();
        SSL_library_init();
        
        conn->sslContext = SSL_CTX_new(SSLv23_client_method());
        if (conn->sslContext == NULL) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        conn->sslHandle = SSL_new(conn->sslContext);
        if (conn->sslHandle == NULL) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        
        if (SSL_set_fd(conn->sslHandle, conn->sock) == 0) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        if (SSL_connect(conn->sslHandle) != 1) {
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }

        printf("SSL Negotiation Successful\n");
    }

    // Recv initial message
    if (irc_recv_flush_to_fp(conn, stdout) < 137)
        irc_recv_flush_to_fp(conn, stdout);
    
    // Send NICK msg
    irc_nick(conn, nick);
    irc_recv_all(conn, &buf, &n);
    if (strncmp(buf, "PING :", 6) == 0)
        irc_pong(conn, buf);
    else
        printf("%s", buf);
    free(buf);

    // Send USER msg
    irc_user(conn, user, "0", realname);
    irc_recv_flush_to_fp(conn, stdout);
    irc_recv_flush_to_fp(conn, stdout);
    if (irc_recv_flush_to_fp(conn, stdout) < 300)
        irc_recv_flush_to_fp(conn, stdout);

    // Send IDENTIFY msg
    asprintf(&buf, "IDENTIFY %s", nickpass);
    irc_msg(conn, "NickServ", buf);
    free(buf);

    return conn;
}


int irc_join(Connection *conn, char *channel)
{
    char *msg;

    asprintf(&msg, "JOIN %s\n", channel);
    irc_send(conn, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_part(Connection *conn, char *channel)
{
    char *msg;

    asprintf(&msg, "PART %s\n", channel);
    irc_send(conn, msg, strlen(msg), 0);

    free(msg);
    return 1;
}


int irc_user(Connection *conn, char *user, char *mode, char *realname)
{
    char *msg;

    asprintf(&msg, "USER %s %s * :%s\n", user, mode, realname);
    irc_send(conn, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_nick(Connection *conn, char *nick)
{
    char *msg;

    asprintf(&msg, "NICK %s\n", nick);
    irc_send(conn, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_msg(Connection *conn, char *rcpt, char *msg)
{
    char *buf;

    asprintf(&buf, "PRIVMSG %s :%s\n", rcpt, msg);
    irc_send(conn, buf, strlen(buf), 0);

    free(buf);
    return 0;
}


int main(int argc, char *argv[])
{
    Connection *conn;
    char *buf = NULL;
    size_t n;
    ssize_t len;
    int i;

    conn = irc_connect(SERVER, PORT, SSL, NICK, NICKPASS, USER, REALNAME);
    irc_join(conn, "#test");
    irc_msg(conn, "#test", "hello");
    irc_recv_flush_to_fp(conn, stdout);
     
    return 0;
}
