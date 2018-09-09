//
// Created by emanuele on 31/08/18.
//

#ifndef PROGETTO_UTILS_H
#define PROGETTO_UTILS_H

#include <limits.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define STR_DIM 1024
#define PRINT_DUMP 1
#define Q_FACTOR 70

struct image_t {
    // Name of current image
    char name[NAME_MAX];
    // Memory mapped of resized image
    size_t size_r;
    struct cache_t *img_c;
    struct image_t *next_img;
};

struct cache_t {
    // quality factor
    int q;
    char img_q[NAME_MAX];
    size_t size_q;
    struct cache_t *next_img_c;
};

// Struct to manage cache hit
struct cache_hit {
    char cache_name[NAME_MAX];
    struct cache_hit *next_hit;
};

// Struct which contains all variables for synchronise threads
struct th_sync {
    struct sockaddr_in client_addr;
    struct cache_hit *cache_hit_tail,
            *cache_hit_head;
    // rappresent the state of a threads associated to a connection:
    // >0 -> socket value
    // -1 -> thread ready for incoming connections
    // -2 -> thread killed by kill_th function or thread not yet created
    // -3 -> newly created thread to be initialized
    int *clients;
    // used to tell to a thread it's slot clients
    volatile int slot_c,
            // number of clients connected
            connections,
            // number of thread active
            th_act,
            th_act_thr,
            to_kill;
    // To manage thread's number and connections (th_act and connections)
    pthread_mutex_t *mutex_th_num;
    // To manage cache access
    pthread_mutex_t *mutex_cache;
    // To sync pthread_condition variables
    pthread_mutex_t *mutex_cond;
    // variables of all threads, array containing condition
    pthread_cond_t *new_c;
    // To initialize threads
    pthread_cond_t *cond_th_init;
    // Number of maximum connection reached
    pthread_cond_t *cond_max_conn;
};

// struct to syn access on cache's resource 
struct cache_syn_t {
    pthread_mutex_t *mtx;
    pthread_cond_t *cond;
    struct cache_hit    *cache_hit_tail, // oldest cache hit
                        *cache_hit_head; // last cache hit
};

//
struct state_syn_t {
    pthread_mutex_t *mtx;
    pthread_cond_t *cond;
    volatile int    init_th_num, // number of thread initialized
                    conn_num;
};

struct th_syn_t {
    pthread_mutex_t *mtx;
    pthread_cond_t *cond;
    int *clients;
    volatile int to_kill;
};

struct accept_conn_syn_t {
    pthread_mutex_t *mtx;
    // array of cond with size MAX_CONN_NUM
    pthread_cond_t *cond;
    struct sockaddr_in *cl_addr;
    volatile int conn_sd;
};

extern int PORT;
extern char *USAGE_MSG;
extern char *USER_OPT;
extern char LOG_PATH[PATH_MAX];
extern char IMG_PATH[PATH_MAX];
extern char TMP_RESIZED_PATH[PATH_MAX];
extern char TMP_CACHE_PATH[PATH_MAX];
extern int MIN_TH_NUM;
extern int MAX_CONN_NUM;
extern int RESIZE_PERC;
extern int cache_space;
extern int LISTEN_SD;
extern FILE *LOG;
extern char *HTML[3];
extern float TH_SCALING_UP;
extern float TH_SCALING_DOWN;

extern struct image_t *IMAGES;
extern struct cache_t *cache;
extern struct cache_syn_t *cache_syn;
extern struct state_syn_t *state_syn;
extern struct th_syn_t *th_syn;
extern struct accept_conn_syn_t *accept_conn_syn;

void error_found(char *msg);
void writef(char *s, FILE *file);
void free_mem();
void rm_dir(char *directory);
void rm_link(char *path);
void check_is_dir(char *path);
void check_is_file(char *path, struct stat *buf);
FILE *open_file(char *path, char *file_name);
void write_log(char *s);
char *get_time(void);
void alloc_res_img(struct image_t **i, char *path);
char *get_img(char *name, size_t img_dim, char *directory);

#endif //PROGETTO_UTILS_H
