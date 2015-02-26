#include "../api/irc.h"

#include <stdio.h>
#include <stdlib.h>

#define MV_ROCK     0
#define MV_PAPER    1
#define MV_SCISSOR  2

#define SRV_WIN     1
#define SRV_TIE     0
#define SRV_LOSS   -1

int rules[3][3] = {{SRV_TIE,  SRV_LOSS, SRV_WIN},
                   {SRV_WIN,  SRV_TIE,  SRV_LOSS},
                   {SRV_LOSS, SRV_WIN,  SRV_TIE}};


int init() {
    return 0;
}

int fini() {
    return 0;
}

int rps_srv_move(char *src) {
    return 1;
}

int handle(char *src, char *dst, char *msg)
{
    char *buf;
    int user_move;

    printf("plugin: %s > %s : %s\n", src, dst, msg);
    if (strcmp(dst, "cbot") == 0) {
        dst = src;
    }

    if (strcmp(msg, ".stats") == 0) {
        // Send stats
        ;
    }
    else if (msg[0] == '.') {
        switch (msg[1]) {
        case 'r':
            user_move = MV_ROCK;
            break;
        case 'p':
            user_move = MV_PAPER;
            break;
        case 's':
            user_move = MV_SCISSOR;
            break;
        default:
            return 0;
        }

        switch (rules[rps_srv_move(src)][user_move]) {
        case SRV_WIN:
            break;
        case SRV_TIE:
            break;
        case SRV_LOSS:
            break;
        }

        asprintf(&buf, "res");
        irc_msg(dst, buf);
        free(buf);
    }

    return 0;
}
