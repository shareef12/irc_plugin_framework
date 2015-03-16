#include "framework.h"
#include "bot.h"

#include <signal.h>

struct list_head bots;


void term_handler(int signum)
{
    bot_t *bot, *n;

    list_for_each_entry_safe(bot, n, &bots, list) {
        list_del(&bot->list);
        bot_destroy(bot, "Terminated by Maintainer");
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


void run_forever()
{
    char *buf, *cmd, *src, *dst, *msg;
    int items;
    size_t n;
    struct list_head *pos;

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
        if (strcmp(src, MAINTAINER) == 0 &&
            strcmp(dst, NICK) == 0 &&
            msg[0] == '!')
        {
            handle_admin_msg(msg);
        }
        else if (list_empty((struct list_head *)plugins)) {
            plugins->handle_func(src,dst,msg);
        }
        else {
            list_for_each(pos, (struct list_head *)plugins) {
                ((plugin_t *)pos)->handle_func(src, dst, msg);
            }
        }

        fflush(stdout);
        free(cmd);
        free(src);
        free(dst);
        free(msg);
        free(buf);
    }
}
*/

int main(int argc, char *argv[])
{
    // Initial globals and register sighandlers
    INIT_LIST_HEAD(&bots);

    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);

    // Initial connection actions
    bot_t *bot = bot_create("shareef12", "beitshlomo.com", "6697", 1, "shareef12", "cbot", "holaa", "shareef12");
    list_add_tail(&bot->list, &bots);

    // Begin the main run loop
    //run_forever();

    bot_destroy(bot, "Finished execution"); 

    return 0;
}
