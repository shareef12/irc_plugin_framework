#ifndef _rps_h
#define _rps_h

#include "../list.h"
#include <stdint.h>

#define MV_ROCK     0
#define MV_PAPER    1
#define MV_SCISSOR  2

#define SRV_WIN     1
#define SRV_TIE     0
#define SRV_LOSS   -1

int rules[3][3] = {{SRV_TIE,  SRV_LOSS, SRV_WIN},
                   {SRV_WIN,  SRV_TIE,  SRV_LOSS},
                   {SRV_LOSS, SRV_WIN,  SRV_TIE}};

typedef struct user_t {
    struct list_head list;
    char *name;
    struct list_head *history;
} user_t;

typedef struct move_t {
    struct list_head list;
    uint8_t user_move;
    uint8_t serv_move;
} move_t;
    

int init();
int fini();
int handle(char *src, char *dst, char *msg);

#endif
