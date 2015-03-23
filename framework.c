#include "api/framework.h"

#include <dlfcn.h>
#include <signal.h>
#include <pthread.h>

#include <netdb.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/err.h>

#define MIN(a,b)    (((a)<(b))?(a):(b))
#define MAX(a,b)    (((a)>(b))?(a):(b))


static void error(bot_t *bot, char *fmt, ...)
{
    va_list va;
    char *err;

    va_start(va, fmt);
    vasprintf(&err, fmt, va);
    va_end(va);

    fflush(stdout);
    fprintf(stderr, "  %s\n", err);
    
    if (bot != NULL)
        irc_msg(bot, bot->maintainer, err);

    free(err);
}


static void status(bot_t *bot, char *fmt, ...)
{
    va_list va;
    char *msg;

    va_start(va, fmt);
    vasprintf(&msg, fmt, va);
    va_end(va);

    printf("  %s\n", msg);
    
    if (bot != NULL)
        irc_msg(bot, bot->maintainer, msg);

    free(msg);
}


static bot_t * get_bot_by_name(char *bot_name)
{
    bot_t *bot;

    list_for_each_entry(bot, &bots, list) {
        if (strcmp(bot->name, bot_name) == 0)
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
 

static int irc_connect(bot_t *bot, char *server, char *port, uint8_t ssl,
                       char *user, char *realname, char *nick, char *pass)
{
    int s;
    struct addrinfo hints, *result, *rp;
    char *buf = malloc(1500);

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    s = getaddrinfo(server, port, &hints, &result);
    if (s != 0) {
        error(NULL, "getaddrinfo: %s", gai_strerror(s));
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
        error(NULL, "Could not bot->ct to %s:%s", server, port);
        return -1;
    }

    freeaddrinfo(result);
    status(NULL, "Connected to %s:%s. Socket fd: %d", server, port, bot->socket);

    bot->sslHandle = NULL;
    bot->sslContext = NULL;
    if (ssl) {
        status(NULL, "Beginning SSL Negotiation...");
        SSL_load_error_strings();
        SSL_library_init();

        bot->sslContext = SSL_CTX_new(SSLv23_client_method());
        if (bot->sslContext == NULL) {
            fprintf(stderr, "  ");
            ERR_print_errors_fp(stderr);
            return -1;
        }

        bot->sslHandle = SSL_new(bot->sslContext);
        if (bot->sslHandle == NULL) {
            fprintf(stderr, "  ");
            ERR_print_errors_fp(stderr);
            return -1;
        }
        if (SSL_set_fd(bot->sslHandle, bot->socket) == 0) {
            fprintf(stderr, "  ");
            ERR_print_errors_fp(stderr);
            return -1;
        }

        if (SSL_connect(bot->sslHandle) != 1) {
            fprintf(stderr, "  ");
            ERR_print_errors_fp(stderr);
            return -1;
        }

        status(NULL, "SSL Negotiation Successful");
    }

    // Recv initial message
    irc_recv_flush_to_fp(bot, stdout);
    irc_recv_flush_to_fp(bot, stdout);

    // Send USER and NICK msg together to avoid awkward PING requests
    snprintf(buf, 1500, "USER %s 0 * :%s\nNICK %s\n", user, realname, nick);
    irc_send(bot, buf, strlen(buf), 0);

    irc_recv(bot, buf, 1500, 0);
    if (strncmp(buf, "PING :", 6) == 0) {
        buf[1] = 'O';
        irc_send(bot, buf, strlen(buf), 0);
    }
    
    irc_recv_flush_to_fp(bot, stdout);
    irc_recv_flush_to_fp(bot, stdout);
    irc_recv_flush_to_fp(bot, stdout);
    irc_recv_flush_to_fp(bot, stdout);
    irc_recv_flush_to_fp(bot, stdout);

    // Send IDENTIFY msg
    snprintf(buf, 1500, "PRIVMSG NickServ :IDENTIFY %s\n", pass);
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

    status(NULL, "%s disconnected", bot->name);
}


static int plugin_load(bot_t *bot, plugin_t *plugin, char *path)
{
    // Open the .so
    plugin->handle = dlopen(path, RTLD_LAZY);
    if (plugin->handle == NULL) {
        error(bot, dlerror());
        return -1;
    }

    // Get the functions that should have been implemented
    plugin->init_func = dlsym(plugin->handle, "init");
    if (plugin->init_func == NULL) {
        error(bot, dlerror());
        dlclose(plugin->handle);
        return -1;
    }

    plugin->handle_func = dlsym(plugin->handle, "handle");
    if (plugin->handle_func == NULL) {
        error(bot, dlerror());
        dlclose(plugin->handle);
        return -1;
    }

    plugin->fini_func = dlsym(plugin->handle, "fini");
    if (plugin->fini_func == NULL) {
        error(bot, dlerror());
        dlclose(plugin->handle);
        return -1;
    }

    // Initialize the plugin
    if (plugin->init_func() < 0) {
        error(bot, "%s initialization failed", path);
        dlclose(plugin->handle);
        return -1;
    }

    return 0;
}


static int plugin_unload(bot_t *bot, plugin_t *plugin)
{
    plugin->fini_func();

    dlclose(plugin->handle);
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

    status(NULL, "Creating bot %s...", name);

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

    status(bot, "%s successfully created", name);

    return bot;
}


void bot_destroy(char *bot_name, char *reason)
{
    bot_t *bot;
    plugin_t *plugin, *p;
    channel_t *channel, *c;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return;
    }
    
    status(bot, "Destroying %s...", bot_name);

    pthread_cancel(bot->thread);
    pthread_join(bot->thread, NULL);

    list_for_each_entry_safe(plugin, p, &bot->plugins, list) {
        plugin_unload(bot, plugin);
    }

    list_for_each_entry_safe(channel, c, &bot->channels, list) {
        // TODO: Free all channels
    }

    irc_disconnect(bot, reason);

    free(bot->name);
    free(bot->server);
    free(bot->port);
    free(bot->user);
    free(bot->nick);
    free(bot->pass);
    free(bot->maintainer);

    list_del(&bot->list);

    status(NULL, "Bot successfully destroyed");
}


void list_bot_names()
{
    bot_t *bot;

    list_for_each_entry(bot, &bots, list)
        printf("  %s\n", bot->name);

    printf("  End of list\n");
}


void bot_list_plugins(char *bot_name)
{
    bot_t *bot;
    plugin_t *plugin;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return;
    }

    list_for_each_entry(plugin, &bot->plugins, list)
        printf("  %s\n", plugin->name);

    printf("  End of list\n");
}


void bot_list_channels(char *bot_name)
{
    bot_t *bot;
    channel_t *channel;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return;
    }

    list_for_each_entry(channel, &bot->channels, list)
        printf("  %s\n", channel->name);

    printf("  End of list\n");
}


int bot_add_plugin(char *bot_name, char *path)
{
    bot_t *bot;
    plugin_t *plugin;
    char *fname;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return -1;
    }

    status(bot, "Loading %s for bot %s...", path, bot_name);

    // Don't load the plugin if it's already loaded
    if (get_plugin_by_path(&bot->plugins, path) != NULL) {
        error(bot, "%s is already loaded", path);
        return -1;  // Plugin already loaded
    }

    plugin = malloc(sizeof(plugin_t));

    if (plugin_load(bot, plugin, path) < 0) {
        free(plugin);
        return -1;  // Could not load the plugin
    }

    plugin->path = strdup(path);
    fname = strrchr(plugin->path, '/');
    if (fname == NULL)
        plugin->name = plugin->path;
    else
        plugin->name = fname + 1;

    list_add_tail(&plugin->list, &bot->plugins);

    status(bot, "%s loaded successfully", path);
    return 0;
}


int bot_remove_plugin(char *bot_name, char *filename)
{
    bot_t *bot;
    plugin_t *plugin;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return -1;
    }

    status(bot, "Unloading %s from %s...", filename, bot_name);

    // Try to find the plugin by path first, because that will be more specific
    plugin = get_plugin_by_path(&bot->plugins, filename);
    if (plugin == NULL)
        plugin = get_plugin_by_name(&bot->plugins, filename);
    if (plugin == NULL) {
        error(bot, "Could not find plugin: %s", filename);
        return -1;
    }

    plugin_unload(bot, plugin);

    status(bot, "%s unloaded", filename);
    return 0;
}


int bot_join_channel(char *bot_name, char *channel)
{
    bot_t *bot;
    channel_t *chan;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return -1;
    }

    asprintf(&msg, "JOIN %s\n", channel);
    irc_send(bot, msg, strlen(msg), 0);
    free(msg);

    chan = malloc(sizeof(channel_t));
    chan->name = strdup(channel);
    list_add_tail(&chan->list, &bot->channels);

    status(bot, "Joined %s", channel);
    return 0;
}


int bot_part_channel(char *bot_name, char *channel, char *reason)
{
    bot_t *bot;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return -1;
    }

    asprintf(&msg, "PART %s :%s\n", channel, reason);
    irc_send(bot, msg, strlen(msg), 0);
    free(msg);

    status(bot, "Left %s", channel);
    return 0;
}


int bot_change_nick(char *bot_name, char *nick)
{
    bot_t *bot;
    char *msg;

    bot = get_bot_by_name(bot_name);
    if (bot == NULL) {
        error(NULL, "Could not find bot: %s", bot_name);
        return -1;
    }

    asprintf(&msg, "NICK %s\n", nick);
    irc_send(bot, msg, strlen(msg), 0);
    free(msg);

    status(bot, "Nick changed to %s", nick);
    return 0;
}







void term_handler(int signum)
{
    bot_t *bot, *n;

    list_for_each_entry_safe(bot, n, &bots, list) {
        bot_destroy(bot->name, "Framework terminated by local admin");
    }
    exit(0);
}


/*
void handle_admin_msg(char *msg)
{
    char *cmd = NULL;
    char *name = NULL;
    int items;
    struct list_head *pos, *n;

    items = sscanf(msg, "%ms %ms", &cmd, &name);

    // LOAD
    if (items == 2 && strcmp(cmd, "!load") == 0) {
        plugin_load(name);
        free(cmd);
        free(name);
    }
    // UNLOAD
    else if (items == 2 && strcmp(cmd, "!unload") == 0) {
        plugin_unload(name);
        free(cmd);
        free(name);
    }
    // RELOAD
    else if (items == 2 && strcmp(cmd, "!reload") == 0) {
        if (strcmp(name, "all") == 0) {
            list_for_each_prev_safe(pos, n, (struct list_head *)plugins) {
                free(name);
                name = strdup(((plugin_t *)pos)->name);
                plugin_unload(name);
                plugin_load(name);
            }
        }
        else {
            plugin_unload(name);
            plugin_load(name);
        }
        free(cmd);
        free(name);
    }
    // LIST
    else if (items == 1 && strcmp(cmd, "!list") == 0) {
        list_for_each(pos, (struct list_head *)plugins) {
            irc_msg(MAINTAINER, ((plugin_t *)pos)->name);
        }
        irc_msg(MAINTAINER, "End of list");
        free(cmd);
    }
    // UNKNOWN
    else {
        fprintf(stderr, "Invalid plugin request: %s\n", msg);
        irc_msg(MAINTAINER, "!(load | unload | reload | list) <module | \"all\">");
        if (cmd != NULL) free(cmd);
        if (name != NULL) free(name);
    }
}
*/


void * run_bot(void *bot)
{
    bot_t *self = bot;
    plugin_t *plugin;
    char buf[1500];
    ssize_t len;
    char *items[3];
    int n;

    while (1) {
        len = irc_recv(self, buf, 1500, 0);
        if (len <= 0)
            continue;
        

        // Process PING
        if (strncmp(buf, "PING :", 6) == 0) {
            buf[1] = 'O';
            irc_send(self, buf, strlen(buf), 0);
            printf("PING: %s", buf);
            continue;
        }

        // Process PRIVMSG
        n = sscanf(buf, ":%m[^!]!%*s PRIVMSG %ms :%m[^\n]\n",
                        &items[0], &items[1], &items[2]);
        if (n == 3) {
            if (strcmp(items[0], self->maintainer) == 0 &&
                strcmp(items[1], self->nick) == 0 &&
                items[2][0] == '!')
            {
                //handle_admin_msg(msg);
                ;
            }
            else {
                list_for_each_entry(plugin, &self->plugins, list) {
                    plugin->handle_func(items[0], items[1], items[2]);
                }
            }
        }

        printf("\r%s\n> ", buf);
        for (n -= 1; n >= 0; n--)
            free(items[n]);
    }
}


void local_terminal()
{
    char *line;
    size_t n;
    ssize_t len;
    int items;
    char *cmd[9];

    while (1) {
        printf("> ");
        fflush(stdout);
        line = NULL;
        len = getline(&line, &n, stdin);
        if (len < 0)
            break;
    
        line[len - 1] = 0;
        items = sscanf(line, "%ms %ms %ms %ms %ms %ms %ms %ms %ms", &cmd[0], &cmd[1],
                      &cmd[2], &cmd[3], &cmd[4], &cmd[5], &cmd[6], &cmd[7], &cmd[8]);

        if (items < 1)
            continue;

        if (strcmp(cmd[0], "ls") == 0) {
            // List bots, plugins, or channels
            if (items == 2 && strncmp(cmd[1], "bots", strlen(cmd[1])) == 0) {
                list_bot_names();
            }
            else if (items == 3 && strncmp(cmd[1], "plugins", strlen(cmd[1])) == 0)
                bot_list_plugins(cmd[2]);
            else if (items == 3 && strncmp(cmd[1], "channels", strlen(cmd[1])) == 0)
                bot_list_channels(cmd[2]);
            else
                printf("  Usage: ls bots\n"
                       "         ls plugins <botname>\n"
                       "         ls channels <botname>\n");
        }
        else if (strncmp(cmd[0], "create", strlen(cmd[0])) == 0) {
            // Create a bot
            if (items == 9) {
                bot_create(cmd[1], cmd[2], cmd[3], strcmp(cmd[4], "0") == 0 ? 0 : 1,
                           cmd[5], cmd[6], cmd[7], cmd[8]);
                printf("Created bot: %s\n", cmd[1]);
            }
            else {
                printf("  Usage: create <name> <server> <port> <ssl> <user>\n"
                       "                <nick> <pass> <maintainer>\n");
            }
        }
        else if (strncmp(cmd[0], "destroy", strlen(cmd[0])) == 0) {
            // Destroy a bot
            if (items == 2) {
                bot_destroy(cmd[1], "Terminated by local admin\n");
            }
            else if (items > 2) {
                bot_destroy(cmd[1], strchr(strchr(line, ' ') + 1, ' '));
            }
            else {
                printf("  Usage: destroy <name> [reason]\n");
            }
        }
        else if (strncmp(cmd[0], "load", MAX(strlen(cmd[0]), 2)) == 0) {
            // Load a plugin
            if (items == 3) {
                bot_add_plugin(cmd[1], cmd[2]);
            }
            else {
                printf("  Usage: load <botname> <plugin_path>\n");
            }
        }
        else if (strncmp(cmd[0], "unload", strlen(cmd[0])) == 0) {
            // Unload a plugin
            if (items == 3) {
                bot_remove_plugin(cmd[1], cmd[2]);
            }
            else {
                printf("  Usage: unload <botname> <plugin_path>\n");
            }
        }
        else if (strncmp(cmd[0], "join", strlen(cmd[0])) == 0) {
            // Join a channel
            if (items == 3) {
                bot_join_channel(cmd[1], cmd[2]);
            }
            else {
                printf("  Usage: join <botname> <channel>\n");
            }
        }
        else if (strncmp(cmd[0], "part", strlen(cmd[0])) == 0) {
            // Part a channel
            if (items == 3) {
                printf("  NOT IMPLEMENTED\n");
            }
            else if (items == 4) {
                printf("  NOT IMPLEMENTED\n");
            }
            else {
                printf("  Usage: part <botname> <channel> [reason]\n");
            }
        }
        else if (items == 1 && strncmp(cmd[0], "quit", strlen(line)) == 0) {
            term_handler(0);
            break;
        } else if (strcmp(line, "help") == 0) {
            printf("  ls bots                 - List all bot currently running\n");
            printf("  ls plugins  <botname>   - List loaded plugins for a bot\n");
            printf("  ls channels <botname>   - List joined channels for a bot\n");
            printf("  create      <name> <server> <port> <ssl> <user> <nick> <pass>\n"
                   "              <maintainer>    - Create a bot\n");
            printf("  destroy     <name> [reason] - Destroy a bot\n");
            printf("  load        <botname> <plugin_path> - Create/Load a plugin for a bot\n");
            printf("  unload      <botname> <plugin_path> - Destroy/Unload a plugin for a bot\n");
            printf("  join        <botname> <channel>          - Join a channel\n");
            printf("  part        <botname> <channel> [reason] - Part from a channel\n");
            printf("  quit        - Terminate the framework\n");
            printf("  help        - Display this help message\n");
        }
        else {
            printf("  Unknown command. Run \"help\" to view usage.\n");
        }

        for (items -= 1; items >= 0; items--) {
            free(cmd[items]);
        }
    }
}


int init_bot()
{
    return -1;
}


int init_bots(char *plugin_conf_dir)
{
    bot_t *bot;

    bot = bot_create("testbot", "beitshlomo.com", "6697", 1, "shareef12", "cbot", "password", "shareef12");
    bot_join_channel(bot->name, "#test");

    // Begin the main run loop
    pthread_create(&bot->thread, NULL, run_bot, bot);

    return 0;
}


int main(int argc, char *argv[])
{
    // Initial globals and register sighandlers
    INIT_LIST_HEAD(&bots);

    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);

    init_bots("");

    local_terminal();

    term_handler(0);
    return 0;
}

