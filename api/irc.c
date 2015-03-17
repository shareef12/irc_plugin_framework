//#include "bot.h"

#include "list.h"

#include <stdio.h>
#include <dlfcn.h>
#include <stdint.h>
#include <string.h>
#include <pthread.h>

#include <netdb.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <openssl/ssl.h>
#include <openssl/err.h>

/**
 * TODO: Redesign nick storage
 * TODO: Implement bot_get_nicks
 */

struct list_head bots;

typedef struct plugin {
    struct list_head list;

    char *name;
    char *path;
    void *handle;
    int (*init_func)();
    int (*handle_func)(char *,char *, char *);
    int (*fini_func)();
} plugin_t;

typedef struct channel {
    struct list_head list;
    
    char *name;
    char *topic;
    char **nicks;
    unsigned int nNicks;
} channel_t;

typedef struct bot {
    struct list_head list;
    
    char *name;
    char *server;
    char *port;
    uint8_t ssl;
    
    char *user;
    char *nick;
    char *pass;
    char *maintainer;
    
    pthread_t thread;
    int socket;
    SSL *sslHandle;
    SSL_CTX *sslContext;
    
    unsigned int nChannels;
    struct list_head channels;
    struct list_head plugins;
} bot_t;


static bot_t * get_bot_by_name(char *bot_name)
{
    bot_t *bot;

    list_for_each_entry(bot, &bots, list) {
        if (strcmp(bot->name, bot_name) == 0)
            return bot;
    }

    return NULL;
}


static bot_t * get_bot_by_thread()
{
    bot_t *bot;
    pthread_t thread = pthread_self();

    list_for_each_entry(bot, &bots, list) {
        if (bot->thread == thread)
            return bot;
    }

    return NULL;
}


static plugin_t * get_plugin_by_name(struct list_head *plugins, char *plugin_name)
{
    plugin_t *plugin;

    list_for_each_entry(plugin, plugins, list) {
        if (strcmp(plugin_name, plugin->name) == 0)
            return plugin;
    }

    return NULL;
}


static plugin_t * get_plugin_by_path(struct list_head *plugins, char *plugin_path)
{
    plugin_t *plugin;

    list_for_each_entry(plugin, plugins, list) {
        if (strcmp(plugin_path, plugin->path) == 0)
            return plugin;
    }

    return NULL;
}
 

static channel_t * get_channel_by_name(struct list_head *channels, char *channel_name)
{
    channel_t *channel;

    list_for_each_entry(channel, channels, list) {
        if (strcmp(channel_name, channel->name) == 0)
            return channel;
    }

    return NULL;
}
    

static ssize_t irc_send(bot_t *bot, char *buf, size_t len, int flags)
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


static ssize_t irc_recv(bot_t *bot, void *buf, size_t len, int flags)
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


static char * irc_recv_all(bot_t *bot)
{
    char *buf = malloc(1500);
    ssize_t buf_size = 0;
    ssize_t len = 0;
    ssize_t n = 0;
    struct timeval timeout = {1, 0};

    setsockopt(bot->socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        if (buf_size - len < 1500) {
            buf = realloc(buf, buf_size + 1500);
            buf_size += 1500;
        }

        n = irc_recv(bot, buf + len, 1500, 0);
        if (n < 0)
            break;

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


static ssize_t irc_recv_flush_to_fp(bot_t *bot, FILE *stream)
{
    char *buf;
    ssize_t len = 0;

    buf = irc_recv_all(bot);
    if (buf != NULL) {
        len = strlen(buf);
        fprintf(stream, "%s", buf);
        free(buf);
    }

    return len;
}


static int irc_connect(bot_t *bot, char *server, char *port, uint8_t ssl,
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
    irc_recv_flush_to_fp(bot, stdout);

    // Send USER and NICK msg together to avoid awkward PING requests
    asprintf(&buf, "USER %s 0 * :%s\nNICK %s\n", user, realname, nick);
    irc_send(bot, buf, strlen(buf), 0);
    free(buf);

    buf = irc_recv_all(bot);
    if (strncmp(buf, "PING :", 6) == 0) {
        buf[1] = 'O';
        irc_send(bot, buf, strlen(buf), 0);
    }
    else {
        printf("%s", buf);
    }
    free(buf);

    irc_recv_flush_to_fp(bot, stdout);

    // Send IDENTIFY msg
    asprintf(&buf, "PRIVMSG NickServ :IDENTIFY %s\n", pass);
    irc_send(bot, buf, strlen(buf), 0);
    free(buf);

    return 0;
}


static void irc_disconnect(bot_t *bot, char *reason)
{
    char *buf;

    asprintf(&buf, "QUIT :%s\n", reason);
    irc_send(bot, buf, strlen(buf), 0);
    free(buf);

    if (bot->sslHandle != NULL) {
        SSL_shutdown(bot->sslHandle);
        SSL_free(bot->sslHandle);
        SSL_CTX_free(bot->sslContext);
    }

    close(bot->socket);
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
    int err;

    bot = malloc(sizeof(bot_t));
    err = irc_connect(bot, server, port, ssl, user, name, nick, pass);
    if (err < 0) {
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

    list_add_tail(&bot->list, &bots);

    return bot;
}


void bot_destroy(char *bot_name, char *reason)
{
    bot_t *bot;
    plugin_t *plugin, *p;
    channel_t *channel, *c;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return;

    irc_disconnect(bot, reason);

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

    list_del(&bot->list);
}


void bot_list_plugins(char *bot_name)
{
    bot_t *bot;
    plugin_t *plugin;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return;

    list_for_each_entry(plugin, &bot->plugins, list)
        printf("%s\n", plugin->name);

    printf("End of list\n");
}


int bot_add_plugin(char *bot_name, char *path)
{
    bot_t *bot;
    plugin_t *plugin;
    char *fname;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return -1;

    // Don't load the plugin if it's already loaded
    if (get_plugin_by_path(&bot->plugins, path) != NULL)
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


int bot_remove_plugin(char *bot_name, char *filename)
{
    bot_t *bot;
    plugin_t *plugin;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return -1;

    // Try to find the plugin by path first, because that will be more specific
    plugin = get_plugin_by_path(&bot->plugins, filename);
    if (plugin == NULL)
        plugin = get_plugin_by_name(&bot->plugins, filename);
    if (plugin == NULL)
        return -1;  // Could not find plugin

    plugin_unload(plugin);
    return 0;
}


void bot_list_channels(char *bot_name)
{
    bot_t *bot;
    channel_t *channel;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return;

    list_for_each_entry(channel, &bot->channels, list)
        printf("%s\n", channel->name);

    printf("End of list\n");
}


int bot_join_channel(char *bot_name, char *channel)
{
    bot_t *bot;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return -1;

    asprintf(&msg, "JOIN %s\n", channel);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


int bot_part_channel(char *bot_name, char *channel, char *reason)
{
    bot_t *bot;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return -1;

    asprintf(&msg, "PART %s :%s\n", channel, reason);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 1;
}


int bot_change_nick(char *bot_name, char *nick)
{
    bot_t *bot;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL)
        return -1;

    asprintf(&msg, "NICK %s\n", nick);
    irc_send(bot, msg, strlen(msg), 0);

    free(msg);
    return 0;
}


void list_bot_names()
{
    bot_t *bot;

    list_for_each_entry(bot, &bots, list)
        printf("%s\n", bot->name);

    printf("End of list\n");
}


char * bot_recv_all()
{
    bot_t *bot;

    bot = get_bot_by_thread();
    if (bot == NULL)
        return NULL;

    return irc_recv_all(bot);
}


ssize_t bot_send(char *buf, size_t len, int flags)
{
    bot_t *bot;
    ssize_t bytesSent;
    
    bot = get_bot_by_thread();
    if (bot == NULL)
        return -1;
    
    return irc_send(bot, buf, len, flags);
}


int bot_msg(char *rcpt, char *fmt, ...)
{
    char *buf, *msg;
    va_list ap;
    
    va_start(ap, fmt);
    asprintf(&buf, "PRIVMSG %s :%s\n", rcpt, fmt);
    vasprintf(&msg, buf, ap);
    va_end(ap);

    bot_send(msg, strlen(msg), 0);

    free(buf);
    free(msg);
    return 0;
}


char * bot_get_server()
{
    bot_t *bot;
    char *buf;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    asprintf(&buf, "%s:%s", bot->server, bot->port);
    return buf;
}


char ** bot_get_channels()
{
    bot_t *bot;
    channel_t *channel;
    char **array;
    int i = 0;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    array = malloc((bot->nChannels + 1) * sizeof(char *));
    list_for_each_entry(channel, &bot->channels, list)
        array[i++] = strdup(channel->name);

    array[i] = NULL;
    return array;
}


char * bot_get_nick()
{
    bot_t *bot;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    return strdup(bot->nick);
}


char ** bot_get_nicks(char *channel_name)
{
    bot_t *bot;
    channel_t *channel;
    
    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }
    
    channel = get_channel_by_name(&bot->channels, channel_name);
    return NULL;
}


char * bot_get_topic(char *channel_name)
{
    bot_t *bot;
    channel_t *channel;

    bot = get_bot_by_thread();
    if (bot == NULL) {
        return NULL;
    }

    channel = get_channel_by_name(&bot->channels, channel_name);
    if (channel == NULL)
        return NULL;

    return channel->topic;
}
