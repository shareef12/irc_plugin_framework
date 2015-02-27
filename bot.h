#ifndef _bot_h
#define _bot_h

#include "list.h"
#include "config.h"

typedef struct plugin_t {
    struct list_head plugins;
    char *name;
    void *handle;
    int (*init_func)();
    int (*handle_func)(char *,char *, char *);
    int (*fini_func)();
} plugin_t;

#endif
