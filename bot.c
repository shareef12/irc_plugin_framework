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

typedef int(*fptr)(char *,char *);
void *plugin;

void term_handler(int signum)
{
    if (plugin != NULL) {
        dlclose(plugin);
    }
    irc_disconnect();
    exit(0);
}


fptr plugin_open(char *filename, char *func)
{
    fptr f;

    plugin = dlopen(filename, RTLD_LAZY);
    if (plugin == NULL) {
        fprintf(stderr, "dlopen: %s\n", dlerror());
        return NULL;
    }

    f = dlsym(plugin, "handle_msg");
    if (f == NULL) {
        fprintf(stderr, "dlsym: %s\n", dlerror());
        return NULL;
    }

    return f;
}


void run_forever(char *pluginName)
{
    char *buf, *cmd, *src, *dst, *msg;
    int items;
    size_t n;

    fptr handle_msg;
    if (pluginName != NULL) {
        handle_msg = plugin_open("", "handle_msg");
    }
    else {
        handle_msg = NULL;
    }

    while (1) {
        buf = NULL;
        cmd = NULL;
        src = NULL;
        dst = NULL;
        msg = NULL;

        irc_recv_all(&buf, &n);
        cmd = strchr(buf, ' ') + 1;

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
        if (strcmp(src, "shareef12") == 0) {
            printf("QWER %s > %s : %s\n", src, dst, msg);
        }
        else  if (handle_msg != NULL) {
            handle_msg(src, msg);
        }
        else {
            printf("%s > %s : %s\n", src, dst, msg);
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
    
    run_forever(NULL);

    irc_disconnect(); 
    return 0;
}
