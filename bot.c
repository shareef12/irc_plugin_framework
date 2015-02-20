#include "irc.h"

#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>

#define SERVER      "beitshlomo.com"
#define PORT        "6697"
#define SSL         1
#define NICK        "cbot"
#define NICKPASS    "holaa"
#define USER        "shareef12"
#define REALNAME    "shareef12"

typedef int(*fptr)(char *,char *,char *);
void *plugin;
fptr handle_func;

void term_handler(int signum)
{
    if (plugin != NULL) {
        dlclose(plugin);
    }
    irc_disconnect();
    exit(0);
}


void plugin_open(char *filename)
{
    plugin = dlopen(filename, RTLD_LAZY);
    if (plugin == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return;
    }

    handle_func = dlsym(plugin, "handle_msg");
    if (handle_func == NULL) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        return;
    }

    printf("Loaded module: %s\n", filename);
}


int handle_msg(char *src, char *dst, char *msg)
{
    printf("%s > %s : %s\n", src, dst, msg);

    return 0;
}


void handle_plugin_msg(char *msg)
{
    char *cmd = NULL;
    char *filename = NULL;
    int items;

    items = sscanf(msg, ".plugin %ms %ms", &cmd, &filename);

    if (items == 1 && strcmp(cmd, "unload") == 0) {
        if (plugin != NULL) {
            dlclose(plugin);
            printf("Unloaded module\n");
            plugin = NULL;
        }
        else {
            irc_msg("shareef12", "No plugin is currently loaded");
        }
        free(cmd);
        handle_func = handle_msg;
    }
    else if (items == 2 && strcmp(cmd, "load") == 0) {
        plugin_open(filename);
        free(cmd);
        free(filename);
    }
    else {
        fprintf(stderr, "Malformed .plugin request: %s\n", msg);
        irc_msg("shareef12", "Invalid .plugin request");
        if (cmd != NULL) free(cmd);
        if (filename != NULL) free(filename);
    }
}


void run_forever(char *pluginName)
{
    char *buf, *cmd, *src, *dst, *msg;
    int items;
    size_t n;

    while (1) {
        buf = NULL;
        cmd = NULL;
        src = NULL;
        dst = NULL;
        msg = NULL;

        irc_recv_all(&buf, &n);

        // Respond to PING first
        if (strncmp(buf, "PING :", 6) == 0) {
            irc_pong(buf);
            free(buf);
            continue;
        }

        // Parse out important PRIVMSG fields
        items = sscanf(buf, ":%m[^!]!%*s %ms %ms :%m[^\n]\n", &src, &cmd, &dst, &msg);

        if (items < 4 || items == EOF || strcmp(cmd, "PRIVMSG") != 0) {
            printf("%s", buf);
            if (cmd != NULL) free(cmd);
            if (src != NULL) free(src);
            if (dst != NULL) free(dst);
            if (msg != NULL) free(msg);
            free(buf);
            continue;
        }

        // Interpret the PRIVMSG
        if (strcmp(src, "shareef12") == 0 &&
            strcmp(dst, NICK) == 0 &&
            strncmp(msg, ".plugin", 7) == 0)
        {
            handle_plugin_msg(msg);
        }
        else {
            handle_func(src, dst, msg);
        }

        fflush(stdout);
        free(cmd);
        free(src);
        free(dst);
        free(msg);
        free(buf);
    }
}


int main(int argc, char *argv[])
{
    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);

    irc_connect(SERVER, PORT, SSL, NICK, NICKPASS, USER, REALNAME);

    irc_join("#test");
    irc_msg("#test", "hello");
    irc_recv_flush_to_fp(stdout);

    plugin = NULL;
    handle_func = handle_msg;
    run_forever(NULL);

    irc_disconnect(); 
    return 0;
}
