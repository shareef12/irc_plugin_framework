#include "irc.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <time.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <openssl/rand.h>
#include <openssl/err.h>

Connection *conn;

ssize_t irc_send(char *buf, size_t len, int flags)
{
    ssize_t bytesSent;
    if (conn->sslHandle == NULL) {
        bytesSent = send(conn->socket, buf, len, flags);
    }
    else {
        bytesSent = SSL_write(conn->sslHandle, buf, len);
    }

    return bytesSent;
}


ssize_t irc_recv(void *buf, size_t len, int flags)
{
    ssize_t bytesRecv;
    if (conn->sslHandle == NULL) {
        bytesRecv = recv(conn->socket, buf, len, flags);
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
ssize_t irc_recv_all(char **bufptr, size_t *n)
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

        received = irc_recv(*bufptr + bytes_read, 1024, 0);
        bytes_read += received;

        if (received < 1024)
            break;
    }

    (*bufptr)[bytes_read] = '\0';
    return bytes_read;
}


ssize_t irc_recv_flush_to_fp(FILE *stream)
{
    char *buf = NULL;
    ssize_t len;
    size_t n;

    len = irc_recv_all(&buf, &n);
    fprintf(stream, "%s", buf);
    free(buf);

    return len;
}


int irc_connect(char *hostname, char *port, int ssl, char *nick,
                         char *nickpass, char *user, char *realname)
{
    int s;
    struct addrinfo hints, *result, *rp;
    char *buf = NULL;
    size_t n;
 
    conn = (Connection *) malloc(sizeof(Connection));
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
        conn->socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (conn->socket == -1)
            continue;
        
        if (connect(conn->socket, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(conn->socket);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not conn->ct to %s:%s\n", hostname, port);
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);
    printf("Connected to %s:%s. Socket fd: %d\n", hostname, port, conn->socket);

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
        
        if (SSL_set_fd(conn->sslHandle, conn->socket) == 0) {
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
    if (irc_recv_flush_to_fp(stdout) < 137)
        irc_recv_flush_to_fp(stdout);
    
    // Send NICK msg
    irc_nick(nick);
    irc_recv_all(&buf, &n);
    if (strncmp(buf, "PING :", 6) == 0)
        irc_pong(buf);
    else
        printf("%s", buf);
    free(buf);

    // Send USER msg
    irc_user(user, "0", realname);
    irc_recv_flush_to_fp(stdout);
    irc_recv_flush_to_fp(stdout);
    if (irc_recv_flush_to_fp(stdout) < 300)
        irc_recv_flush_to_fp(stdout);

    // Send IDENTIFY msg
    asprintf(&buf, "IDENTIFY %s", nickpass);
    irc_msg("NickServ", buf);
    free(buf);

    return 0;
}


void irc_disconnect(char *reason)
{
    irc_quit(reason);
    if (conn->sslHandle != NULL) {
        SSL_shutdown(conn->sslHandle);
        SSL_free(conn->sslHandle);
        SSL_CTX_free(conn->sslContext);
    }

    close(conn->socket);
    free(conn);
}


int irc_quit(char *reason)
{
    char *msg;

    asprintf(&msg, "QUIT :%s\n", reason);
    irc_send(msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_join(char *channel)
{
    char *msg;

    asprintf(&msg, "JOIN %s\n", channel);
    irc_send(msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_part(char *channel)
{
    char *msg;

    asprintf(&msg, "PART %s\n", channel);
    irc_send(msg, strlen(msg), 0);

    free(msg);
    return 1;
}


int irc_user(char *user, char *mode, char *realname)
{
    char *msg;

    asprintf(&msg, "USER %s %s * :%s\n", user, mode, realname);
    irc_send(msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_nick(char *nick)
{
    char *msg;

    asprintf(&msg, "NICK %s\n", nick);
    irc_send(msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int irc_msg(char *rcpt, char *fmt, ...)
{
    char *buf, *msg;
    va_list ap;
    
    va_start(ap, fmt);
    asprintf(&buf, "PRIVMSG %s :%s\n", rcpt, fmt);
    vasprintf(&msg, buf, ap);

    irc_send(msg, strlen(msg), 0);

    free(buf);
    free(msg);
    return 0;
}


int irc_pong(char *ping)
{
    ping[1] = 'O';
    irc_send(ping, strlen(ping), 0);

    return 0;
}
