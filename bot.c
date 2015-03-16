#include "bot.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/socket.h>
#include <netdb.h>
#include <openssl/rand.h>
#include <openssl/err.h>

#include <dlfcn.h>

/**
 * TODO: Implement channel info tracking/joining/parting
 */


static ssize_t bot_send(bot_t *bot, char *buf, size_t len, int flags)
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


static ssize_t bot_recv(bot_t *bot, void *buf, size_t len, int flags)
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


static char * bot_recv_all(bot_t *bot)
{
    char *buf = malloc(1500);
    ssize_t buf_size = 0;
    ssize_t len = 0, n = 0;
    struct timeval timeout = {1, 0};
    
    setsockopt(bot->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        if (buf_size - len < 1500) {
            buf = realloc(buf, buf_size + 1500);
            buf_size += 1500;
        }

        n = bot_recv(bot, buf + len, 1500, 0);
        if (n < 0) {
            break;
        }
        len += n;
    }

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    setsockopt(bot->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    if (len == 0) {
        free(buf);
        return NULL;
    }
    
    buf[len] = 0;
    return buf;
}


static ssize_t bot_recv_flush_to_fp(bot_t *bot, FILE *stream)
{
    char *buf;
    size_t len = 0;

    buf = bot_recv_all(bot);
    if (buf != NULL) {
        len = strlen(buf);
        fprintf(stream, "%s", buf);
        free(buf);
    }

    return len;
}


static int bot_connect(bot_t *bot, char *server, char *port, uint8_t ssl,
                       char *user, char *realname, char *nick, char *pass)
{
    int s;
    struct addrinfo hints, *result, *rp;
    char *buf = NULL;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    s = getaddrinfo(server, port, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        return -1;
    }

    for (rp = result ; rp != NULL; rp = rp->ai_next) {
        bot->socket = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (bot->socket == -1)
            continue;

        if (connect(bot->socket, rp->ai_addr, rp->ai_addrlen) == 0)
            break;

        close(bot->socket);
    }

    if (rp == NULL) {
        fprintf(stderr, "Could not bot->ct to %s:%s\n", server, port);
        return -1;
    }

    freeaddrinfo(result);
    printf("Connected to %s:%s. Socket fd: %d\n", server, port, bot->socket);

    bot->sslHandle = NULL;
    bot->sslContext = NULL;
    if (ssl) {
        printf("Beginning SSL Negotiation...\n");
        SSL_load_error_strings();
        SSL_library_init();

        bot->sslContext = SSL_CTX_new(SSLv23_client_method());
        if (bot->sslContext == NULL) {
            ERR_print_errors_fp(stderr);
            return -1;
        }

        bot->sslHandle = SSL_new(bot->sslContext);
        if (bot->sslHandle == NULL) {
            ERR_print_errors_fp(stderr);
            return -1;
        }
        if (SSL_set_fd(bot->sslHandle, bot->socket) == 0) {
            ERR_print_errors_fp(stderr);
            return -1;
        }

        if (SSL_connect(bot->sslHandle) != 1) {
            ERR_print_errors_fp(stderr);
            return -1;
        }

        printf("SSL Negotiation Successful\n");
    }

    // Recv initial message
    bot_recv_flush_to_fp(bot, stdout);

    // Send USER and NICK msg together to avoid awkward PING requests
    asprintf(&buf, "USER %s 0 * :%s\nNICK %s\n", user, realname, nick);
    bot_send(bot, buf, strlen(buf), 0);
    free(buf);

    buf = bot_recv_all(bot);
    if (strncmp(buf, "PING :", 6) == 0) {
        buf[1] = 'O';
        bot_send(bot, buf, strlen(buf), 0);
    }
    else {
        printf("%s", buf);
    }
    free(buf);

    bot_recv_flush_to_fp(bot, stdout);

    // Send IDENTIFY msg
    asprintf(&buf, "PRIVMSG NickServ :IDENTIFY %s\n", pass);
    bot_send(bot, buf, strlen(buf), 0);
    free(buf);

    bot_send(bot, "JOIN #test\n", 11, 0);
    bot_recv_flush_to_fp(bot, stdout);

    return 0;
}


static void bot_disconnect(bot_t *bot, char *reason)
{
    char *buf;

    asprintf(&buf, "QUIT :%s\n", reason);    
    bot_send(bot, buf, strlen(buf), 0);
    free(buf);

    if (bot->sslHandle != NULL) {
        SSL_shutdown(bot->sslHandle);
        SSL_free(bot->sslHandle);
        SSL_CTX_free(bot->sslContext);
    }

    close(bot->socket);
}


static plugin_t * plugin_get_by_name(struct list_head *plugins, char *name)
{
    plugin_t *plugin;
    
    list_for_each_entry(plugin, plugins, list) {
        if (strcmp(plugin->name, name) == 0)
            return plugin;
    }

    return NULL;
}


static plugin_t * plugin_get_by_path(struct list_head *plugins, char *path)
{
    plugin_t *plugin;

    list_for_each_entry(plugin, plugins, list) {
        if (strcmp(plugin->path, path) == 0)
            return plugin;
    }

    return NULL;
}


static int plugin_load(plugin_t *plugin, char *path)
{
    // Open the .so
    plugin->handle = dlopen(path, RTLD_LAZY);
    if (plugin->handle == NULL) {
        return -1;
    }

    // Get the functions that should have been implemented
    plugin->init_func = dlsym(plugin->handle, "init");
    if (plugin->init_func == NULL) {
        dlclose(plugin->handle);
        return -1;
    }
    
    plugin->handle_func = dlsym(plugin->handle, "handle");
    if (plugin->handle_func == NULL) {
        dlclose(plugin->handle);
        return -1;
    }

    plugin->fini_func = dlsym(plugin->handle, "fini");
    if (plugin->fini_func == NULL) {
        dlclose(plugin->handle);
        return -1;
    }

    // Initialize the plugin
    if (plugin->init_func() < 0) {
        dlclose(plugin->handle);
        return -1;
    }

    return 0;
}


static int plugin_unload(plugin_t *plugin)
{
    plugin->fini_func();
    
    dlclose(plugin->handle);
    free(plugin->name);
    free(plugin->path);
    
    list_del(&plugin->list);
    free(plugin);

    return 0;
}



bot_t * bot_create(char *name, char *server, char *port, uint8_t ssl,
                   char *user, char *nick, char *pass, char *maintainer)
{
    bot_t *bot;

    bot = malloc(sizeof(bot_t));
    if (bot_connect(bot, server, port, ssl, user, name, nick, pass) < 0) {
        free(bot);
        return NULL;
    }

    bot->name   = strdup(name);
    bot->server = strdup(server);
    bot->port   = strdup(port);
    bot->ssl    = ssl;
    bot->user   = strdup(user);
    bot->nick   = strdup(nick);
    bot->pass   = strdup(pass);
    bot->maintainer = strdup(maintainer);

    INIT_LIST_HEAD(&bot->channels);
    INIT_LIST_HEAD(&bot->plugins);

    return bot;
}


void bot_destroy(bot_t *bot, char *reason)
{
    plugin_t *plugin, *p;
    channel_t *channel, *c;
    
    bot_disconnect(bot, reason);

    free(bot->name);
    free(bot->server);
    free(bot->port);
    free(bot->user);
    free(bot->nick);
    free(bot->pass);
    free(bot->maintainer);

    list_for_each_entry_safe(plugin, p, &bot->plugins, list) {
        plugin_unload(plugin);
    }

    list_for_each_entry_safe(channel, c, &bot->channels, list) {
        // TODO: Free all channels
    }
}


int bot_add_plugin(bot_t *bot, char *path)
{
    plugin_t *plugin;
    char *fname;

    // Don't load the plugin if it's already loaded
    if (plugin_get_by_path(&bot->plugins, path) != NULL)
        return -1;  // Plugin already loaded

    plugin = malloc(sizeof(plugin_t));
    
    if (plugin_load(plugin, path) < 0) {
        free(plugin);
        return -1;  // Could not load the plugin
    }

    plugin->path = strdup(path);
    fname = strrchr(path, '/');
    if (fname == NULL)
        plugin->name = path;
    else
        plugin->name = fname + 1;

    list_add_tail(&plugin->list, &bot->plugins);
    return 0;
}

    
int bot_remove_plugin(bot_t *bot, char *identifier)
{
    plugin_t *plugin;

    // Try to find the plugin by path first, because that will be more specific
    plugin = plugin_get_by_path(&bot->plugins, identifier);
    if (plugin == NULL)
        plugin = plugin_get_by_name(&bot->plugins, identifier);
    if (plugin == NULL)
        return -1;  // Could not find plugin

    plugin_unload(plugin);
    return 0;
}   

