#include "rps.h"
#include "../api/irc.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <glob.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>

struct list_head *users;

static user_t * get_user_by_name(char *name)
{
    user_t *user;
    struct list_head *pos;

    list_for_each(pos, users) {
        user = (user_t *)pos;
        if (strcmp(user->name, name) == 0) {
            return user;
        }
    }
    
    return NULL;
}


static user_t * create_user(char *name)
{
    user_t *user;

    user = (user_t *) malloc(sizeof(user_t));
    user->name = strdup(name);
    user->history = (struct list_head *) malloc(sizeof(struct list_head));
    INIT_LIST_HEAD((struct list_head *)user);
    INIT_LIST_HEAD(user->history);

    return user;
}


static void destroy_user(user_t *user)
{
    struct list_head *pos, *n;

    free(user->name);
    list_for_each_safe(pos, n, user->history) {
        list_del(pos);
        free(pos);
    }
    free(user->history);
    list_del((struct list_head *)user);
    free(user);
}


static move_t * create_move(uint8_t user_move, uint8_t serv_move)
{
    move_t *move;

    move = (move_t *) malloc(sizeof(move_t));
    move->user_move = user_move;
    move->serv_move = serv_move;

    return move;
}


static int add_move_for_user(char *name, uint8_t user_move, uint8_t serv_move)
{
    user_t *user;
    move_t *move;

    user = get_user_by_name(name);
    if (user == NULL) {
        user = create_user(name);
        list_add_tail(&user->list, users);
    }

    move = create_move(user_move, serv_move);
    list_add_tail(&move->list, user->history);

    return 0;
}


static int save_history()
{
    FILE *fp;
    char *fname;
    user_t *user;
    move_t *game;
    struct list_head *pos, *pos2;

    list_for_each(pos, users) {
        user = (user_t *)pos;
        asprintf(&fname, "%s.hist", user->name);
        fp = fopen(fname, "w");
        free(fname);

        list_for_each(pos2, user->history) {
            game = (move_t *)pos2;
            fputc(game->user_move + '0', fp);
            fputc(game->serv_move + '0', fp);
            fputc(' ', fp);
        }
        fclose(fp);
    }

    return 0;
}


static int load_history()
{
    glob_t gl;
    FILE *fp;
    int i;
    char game[4], *delim;
    user_t *user;
    move_t *move;

    glob("/tmp/rps/*.hist", 0, NULL, &gl);

    for (i = 0; i < gl.gl_pathc; i++) {
        fp = fopen(gl.gl_pathv[i], "r");
        delim = strchr(gl.gl_pathv[i], '.');
        *delim = 0;

        user = create_user(gl.gl_pathv[i]);
        list_add_tail(&user->list, users);

        while (fread(game, 1, 3, fp) == 3) {
            move = create_move(game[0], game[1]);
            list_add_tail(&move->list, user->history);
        }

        fclose(fp);
    }

    globfree(&gl);
    return 0;
}


static int rps_srv_move(char *src)
{
    //user_t *opp;
    struct timeval tv;

    //opp = get_user_by_name(src);
    if (1 == 1) {
    //if (opp == NULL) {
        gettimeofday(&tv, NULL);
        srandom(tv.tv_sec ^ tv.tv_usec);
        return random() % 3;
    }
    
    return -1;
}


static int send_all_stats(char *dst)
{
    struct list_head *pos;
    user_t *user;

    list_for_each(pos, users) {
        user = (user_t *)pos;
        irc_msg(dst, "%s: W:%lu L:%lu T:%lu %%:%lu\n", user->name,
                    user->wins, user->losses, user->ties,
                    user->wins * 100 / (user->wins + user->losses));
    }

    return 0;
}


int init()
{
    users = (struct list_head *) malloc(sizeof(struct list_head));
    INIT_LIST_HEAD((struct list_head *)users);

    if (mkdir("/tmp/rps/", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
        if (errno == EEXIST) {
            load_history();
        }
        else {
            perror("mkdir");
            return -1;
        }
    }
    return 0;
}


int fini()
{
    struct list_head *pos, *n;

    save_history();
    list_for_each_safe(pos, n, users) {
        destroy_user((user_t *)pos);
    }    
    free(users);
    return 0;
}


int handle(char *src, char *dst, char *msg)
{
    int user_move;
    int serv_move;
    user_t *opp;

    printf("plugin: %s > %s : %s\n", src, dst, msg);
    if (strcmp(dst, "cbot") == 0) {
        dst = src;

        if (strncmp(msg, ".stats", 6) == 0) {
            send_all_stats(dst);
            return 0;
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
            
            serv_move = rps_srv_move(src);
            add_move_for_user(src, user_move, serv_move);
            opp = get_user_by_name(src);

            switch (rules[serv_move][user_move]) {
            case SRV_WIN:
                irc_msg(dst, "You lose!");
                opp->losses += 1;
                break;
            case SRV_TIE:
                irc_msg(dst, "Tied...");
                opp->ties += 1;
                break;
            case SRV_LOSS:
                irc_msg(dst, "You win!");
                opp->wins += 1;
                break;
            }
    
            return 0;
        }
  
    }
    else if (strcmp(msg, ".stats") == 0) {
        send_all_stats(dst);
    }

    return 0;
}
