#include "api/framework.h"

#include <pthread.h>
#include <signal.h>

#define MIN(a,b)    (((a)<(b))?(a):(b))
#define MAX(a,b)    (((a)>(b))?(a):(b))


static bot_t * get_bot_by_name(char *bot_name)
{
    bot_t *bot;

    list_for_each_entry(bot, &bots, list) {
        if (strcmp(bot->name, bot_name) == 0)
            return bot;
    }
    
    return NULL;
}


void term_handler(int signum)
{
    bot_t *bot, *n;

    list_for_each_entry_safe(bot, n, &bots, list) {
        // TODO: Wait for the thread to join
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


void * run_forever(void *bot)
{
    char *buf, *cmd, *src, *dst, *msg;
    bot_t *self = bot;
    int items;

    while (1) {
        buf = NULL;
        cmd = NULL;
        src = NULL;
        dst = NULL;
        msg = NULL;

        buf = bot_recv_all();
        if (buf == NULL)
            continue;

        // Respond to PING first
        if (strncmp(buf, "PING :", 6) == 0) {
            buf[1] = 'O';
            bot_send(buf, strlen(buf), 0);
            printf("PING: %s", buf);
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

        printf("%s\n", buf);

        /*
        // Interpret the PRIVMSG
        if (strcmp(src, MAINTAINER) == 0 &&
            strcmp(dst, NICK) == 0 &&
            msg[0] == '!')
        {
            //handle_admin_msg(msg);
        }
        
        else if (list_empty((struct list_head *)plugins)) {
            //plugins->handle_func(src,dst,msg);
        }
        else {
            list_for_each(pos, (struct list_head *)plugins) {
                ((plugin_t *)pos)->handle_func(src, dst, msg);
            }
        }
        */

        fflush(stdout);
        free(cmd);
        free(src);
        free(dst);
        free(msg);
        free(buf);
    }
}


void terminal()
{
    char *line;
    size_t n;
    ssize_t len;
    int items;
    char *cmd[8];

    while (1) {
        printf("> ");
        line = NULL;
        len = getline(&line, &n, stdin);
        if (len < 0)
            break;
    
        line[len - 1] = 0;
        items = sscanf(line, "%ms %ms %ms %ms %ms %ms %ms %ms", &cmd[0], &cmd[1],
                      &cmd[2], &cmd[3], &cmd[4], &cmd[5], &cmd[6], &cmd[7]);

        if (items < 1)
            continue;

        if (strcmp(cmd[0], "ls") == 0) {
            if (items == 2 && strncmp(cmd[1], "bots", strlen(cmd[1])) == 0) {
                list_bot_names();
            }
            else if (items == 3 && strncmp(cmd[1], "plugins", strlen(cmd[1])) == 0)
                // list plugins with cmd[2]
                printf("  NOT IMPLEMENTED\n");
            else if (items == 3 && strncmp(cmd[1], "channels", strlen(cmd[1])) == 0)
                // list channels with cmd[2]
                printf("  NOT IMPLEMENTED\n");
            else
                printf("  Usage: ls bots\n"
                       "         ls plugins <botname>\n"
                       "         ls channels <botname>\n");
        }
        else if (strncmp(cmd[0], "create", strlen(cmd[0])) == 0) {
            // Create a bot
            if (items == 8) {
                printf("  NOT IMPLEMENTED\n");
            }
            else {
                printf("  Usage: create <name> <server> <port> <ssl> <user>\n"
                       "                <nick> <pass> <maintainer>\n");
            }
        }
        else if (strncmp(cmd[0], "destroy", strlen(cmd[0])) == 0) {
            // Destroy a bot
            if (items == 2) {
                printf("  NOT IMPLEMENTED\n");
            }
            else {
                printf("  Usage: destroy <name>\n");
            }
        }
        else if (strncmp(cmd[0], "load", MAX(strlen(cmd[0]), 2)) == 0) {
            // Load a plugin
            if (items == 3) {
                printf("  NOT IMPLEMENTED\n");
            }
            else {
                printf("  Usage: load <botname> <plugin_path>\n");
            }
        }
        else if (strncmp(cmd[0], "unload", strlen(cmd[0])) == 0) {
            // Unload a plugin
            if (items == 3) {
                printf("  NOT IMPLEMENTED\n");
            }
            else {
                printf("  Usage: unload <botname> <plugin_path>\n");
            }
        }
        else if (strncmp(cmd[0], "join", strlen(cmd[0])) == 0) {
            // Join a channel
            if (items == 3) {
                printf("  NOT IMPLEMENTED\n");
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
            printf("  Terminating...\n");
            term_handler(0);
            break;
        } else if (strcmp(line, "help") == 0) {
            printf("  ls bots - List all bot currently running\n");
            printf("  ls plugins <botname> - List loaded plugins for a bot\n");
            printf("  ls channels <botname> - List joined channels for a bot\n");
            printf("  create <name> <server> <port> <ssl> <user> <nick> <pass>\n"
                   "         <maintainer> - Create a bot\n");
            printf("  destroy <name> - Destroy a bot\n");
            printf("  load <botname> <plugin_path> - Create/Load a plugin for a bot\n");
            printf("  unload <botname> <plugin_path> - Destroy/Unload a plugin for a bot\n");
            printf("  join <botname> <channel> - Join a specified channel\n");
            printf("  part <botname> <channel> [reason] - Part from a channel\n");
            printf("  quit - Terminate the framework\n");
            printf("  help - Display this help message\n");
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
    pthread_create(&bot->thread, NULL, run_forever, bot);

    return 0;
}


int main(int argc, char *argv[])
{
    // Initial globals and register sighandlers
    INIT_LIST_HEAD(&bots);

    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);

    init_bots("");

    terminal();

    term_handler(0);
    return 0;
}
