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
char *plugin_name;
fptr  handle_func;

void term_handler(int signum)
{
    if (plugin != NULL) {
        dlclose(plugin);
        free(plugin_name);
    }
    irc_disconnect();
    exit(0);
}


int handle_msg(char *src, char *dst, char *msg)
{
    printf("%s > %s : %s\n", src, dst, msg);

    return 0;
}


int plugin_load(char *filename)
{
    void *p;
    fptr f;

    p = dlopen(filename, RTLD_LAZY);
    if (p == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return -1;
    }

    f = dlsym(p, "handle_msg");
    if (f == NULL) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        dlclose(p);
        return -1;
    }

    // Successfully loaded the plugin
    if (plugin != NULL) {
        dlclose(plugin);
        free(plugin_name);
    }
    plugin = p;
    plugin_name = strdup(filename);
    handle_func = f;
    return 0;
}


int plugin_unload()
{
    if (plugin == NULL) {
        fprintf(stderr, "No plugin loaded\n");
        return -1;
    }

    if (dlclose(plugin) != 0) {
        fprintf(stderr, "dlclose %s\n", dlerror());
        return -1;
    }
    free(plugin_name);

    plugin = NULL;
    plugin_name = NULL;
    handle_func = handle_msg;

    return 0;
}


void handle_plugin_msg(char *msg)
{
    char *cmd = NULL;
    char *filename = NULL;
    char *buf;
    int items;

    items = sscanf(msg, "%ms %ms", &cmd, &filename);

    // UNLOAD
    if (items == 1 && strcmp(cmd, ".unload") == 0) {
        if (plugin_unload() < 0) {
            printf("Plugin unload failed\n");
            irc_msg("shareef12", "Plugin unload failed");
        }
        else {
            printf("Plugin unloaded\n");
            irc_msg("shareef12", "Plugin unloaded");
        }
        free(cmd);
    }
    // LOAD
    else if (items == 2 && strcmp(cmd, ".load") == 0) {
        if (plugin_load(filename) < 0) {
            printf("Plugin load failed\n");
            irc_msg("shareef12", "Plugin load failed");
        }
        else {
            printf("Plugin %s loaded\n", filename);
            asprintf(&buf, "Plugin %s loaded", filename);
            irc_msg("shareef12", buf);
            free(buf);
        }
        free(cmd);
        free(filename);
    }
    // RELOAD
    else if (items == 1 && strcmp(cmd, ".reload") == 0) {
        if (plugin_load(plugin_name) < 0) {
            printf("Plugin reload failed\n");
            irc_msg("shareef12", "Plugin reload failed");
        }
        else {
            printf("Plugin %s reloaded\n", plugin_name);
            asprintf(&buf, "Plugin %s reloaded", plugin_name);
            irc_msg("shareef12", buf);
            free(buf);
        }
        free(cmd);
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
            msg[0] == '.')
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
    plugin_name = NULL;
    handle_func = handle_msg;
    run_forever(NULL);

    irc_disconnect(); 
    return 0;
}
