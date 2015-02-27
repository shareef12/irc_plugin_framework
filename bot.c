#include "bot.h"
#include "api/irc.h"

#include <unistd.h>
#include <signal.h>
#include <dlfcn.h>

plugin_t *plugins;


void term_handler(int signum)
{
    struct list_head *pos, *n;
    plugin_t *p;

    list_for_each_safe(pos, n, (struct list_head *)plugins) {
        list_del(pos);
        p = (plugin_t *)pos;
        dlclose(p->handle);
        free(p->name);
        free(p);
    }
    irc_disconnect("Terminated by maintainer");
    exit(0);
}


int default_msg_handler(char *src, char *dst, char *msg)
{
    printf("%s > %s : %s\n", src, dst, msg);

    return 0;
}


static plugin_t * get_plugin_by_name(char *name)
{
    struct list_head *pos;
    plugin_t *p;

    list_for_each(pos, (struct list_head *)plugins) {
        p = (plugin_t *)pos;
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }

    return NULL;
}


static int plugin_load_find_funcs(plugin_t *p)
{
    char *err = NULL;
    
    p->init_func = dlsym(p->handle, "init");
    if (p->init_func == NULL) {
        err = dlerror();
        fprintf(stderr, "%s\n", err);
        irc_msg(MAINTAINER, "%s", err);
        return -1;
    }
    
    p->handle_func = dlsym(p->handle, "handle");
    if (p->handle_func == NULL) {
        err = dlerror();
        fprintf(stderr, "%s\n", err);
        irc_msg(MAINTAINER, "%s", err);
        return -1;
    }

    p->fini_func = dlsym(p->handle, "fini");
    if (p->fini_func == NULL) {
        err = dlerror();
        fprintf(stderr, "%s\n", err);
        irc_msg(MAINTAINER, "%s", err);
        return -1;
    }

    return 0;
}


int plugin_load(char *name)
{
    plugin_t *p = (plugin_t *) malloc(sizeof(plugin_t));
    char *err;

    // Do not open the same plugin twice
    if (get_plugin_by_name(name)) {
        fprintf(stderr, "Plugin %s is already loaded\n", name);
        irc_msg(MAINTAINER, "Plugin %s is already loaded", name);
        free(p);
        return -1;
    }

    // Open the .so
    p->handle = dlopen(name, RTLD_LAZY);
    if (p->handle == NULL) {
        err = dlerror();
        fprintf(stderr, "%s\n", err);
        irc_msg(MAINTAINER, "%s", err);
        free(p);
        return -1;
    }

    // Get the functions that should have been implemented
    if (plugin_load_find_funcs(p) < 0) {
        dlclose(p->handle);
        free(p);
        return -1;
    }

    // Initialize the plugin
    if (p->init_func() == -1) {
        fprintf(stderr, "%s initialization failed\n", name);
        irc_msg(MAINTAINER, "%s initialization failed", name);
        dlclose(p->handle);
        free(p);
        return -1;
    }

    // Add the plugin to the master list
    p->name = strdup(name);
    list_add_tail((struct list_head *)p, (struct list_head *)plugins);

    printf("Loaded plugin %s\n", p->name);
    irc_msg(MAINTAINER, "Loaded plugin %s", p->name);
    return 0;
}


int plugin_unload(char *name)
{
    plugin_t *p;
    char *err;

    p = get_plugin_by_name(name);
    if (p == NULL) {
        fprintf(stderr, "Plugin %s was not loaded\n", name);
        irc_msg(MAINTAINER, "Plugin %s was not loaded", name);
        return -1;
    }

    p->fini_func();
    if (dlclose(p->handle) != 0) {
        err = dlerror();
        fprintf(stderr, "%s\n", err);
        irc_msg(MAINTAINER, "%s", err);
    }

    free(p->name);
    list_del((struct list_head *)p);
    free(p);

    printf("Unloaded plugin %s\n", name);
    irc_msg(MAINTAINER, "Unloaded plugin %s", name);
    return 0;
}


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


int main(int argc, char *argv[])
{
    plugin_t def;

    // Initial globals and register sighandlers
    def.name = "default";
    def.init_func = NULL;
    def.fini_func = NULL;
    def.handle_func = default_msg_handler;
    INIT_LIST_HEAD((struct list_head *)&def);
    plugins = &def;

    signal(SIGINT, term_handler);
    signal(SIGTERM, term_handler);

    // Initial connection actions
    irc_connect(SERVER, PORT, SSL_CONN, NICK, NICKPASS, USER, REALNAME);
    irc_join(CHANNELS);
    irc_msg("#test", "hello");
    irc_recv_flush_to_fp(stdout);

    if (argc > 2) {
        fprintf(stderr, "Usage: %s <plugin>\n", argv[0]);
        exit(1);
    }
    else if (argc == 2) {
       plugin_load(argv[1]);
    }

    // Begin the main run loop
    run_forever();
    irc_disconnect("Finished execution"); 

    return 0;
}
