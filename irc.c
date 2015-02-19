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
#define  USER       ""


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


static void irc_recv_flush_to_fp(Connection *conn, FILE *stream)
{
    char *buf = NULL;
    ssize_t len;
    size_t n;

    len = irc_recv_all(conn, &buf, &n);
    fprintf(stream, "%s", buf);
    free(buf);
}


Connection * irc_connect(char *hostname, char *port, int ssl, char *nick)
{
    int s;
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

    // Send NICK msg
    sleep(2);
    irc_recv_flush_to_fp(conn, stdout);
    irc_nick(conn, nick);
    
    // Respond to the server's PING
    sleep(1);
    irc_recv_all(conn, &buf, &n);
    printf("%s", buf);
    buf[1] = 'O';
    printf("%s", buf);
    irc_send(conn, buf, strlen(buf), 0);
    free(buf);

    // Send USER msg
    buf = "USER shareef12 0 * :shareef12\n";
    irc_send(conn, buf, strlen(buf), 0);

    // Send IDENTIFY msg
    buf = "PRIVMSG NickServ :identify holaa\n";
    irc_send(conn, buf, strlen(buf), 0);
    
    sleep(2);
    irc_recv_flush_to_fp(conn, stdout);
    sleep(2);
    irc_recv_flush_to_fp(conn, stdout);

    return conn;
}


int irc_join(Connection *conn, char *channel)
{
    return 1;
}


int irc_part(Connection *conn, char *channel)
{
    return 1;
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
    return 1;
}


int main(int argc, char *argv[])
{
    Connection *conn;
    char *buf = NULL;
    size_t n;
    ssize_t len;
    int i;

    conn = irc_connect(SERVER, PORT, SSL, NICK);
     
    return 0;
}
