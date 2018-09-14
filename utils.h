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
#define Q_FACTOR 100

// struct that contained all information about an resized's image
struct image_t {
    // Name of current image
    char name[NAME_MAX];
    // Memory mapped of resized image
    size_t size_r;
    // struct for information of that image in cache
    struct cache_t *img_c;
    // next image
    struct image_t *next_img;
};

// struct that contained all information about an image saved in cache
struct cache_t {
    // quality factor
    int q;
    // Name of image in cache
    char img_q[NAME_MAX];
    // Memory mapped of image in cache
    size_t size_q;
    // next image in cache
    struct cache_t *next_img_c;
};

// Struct to manage cache hit
struct cache_hit {
    // Name of image in cache
    char cache_name[NAME_MAX];
    // next cache hit
    struct cache_hit *next_hit;
};

// struct to syn access on cache's resource 
struct cache_syn_t {
    pthread_mutex_t *mtx;
    pthread_cond_t *cond;
    struct cache_hit    *cache_hit_tail, // oldest cache hit
                        *cache_hit_head; // last cache hit
};

// Struct that contained information of server's state
struct state_syn_t {
    pthread_mutex_t *mtx;
    pthread_cond_t *cond;
    volatile int    init_th_num,    // number of thread initialized
                    conn_num;       // number of connection
};

// struct use to syncronize thread
struct th_syn_t {
    pthread_mutex_t *mtx;
    // array of cond with size MAX_CONN_NUM
    pthread_cond_t *cond;
    struct sockaddr_in *cl_addr;    // client address of new connection
    int *clients;                   // array of connection socket descriptor with size MAX_CONN_NUM
                                    // if -1 thread associated is waiting
                                    // if -2 there is no thread initialize associated
    int accept;                     // boolean to comunicate at main thread that thread worker has accepted the new
                                    // connection
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
extern int TH_SCALING_UP;
extern int TH_SCALING_DOWN;

extern struct image_t *IMAGES;
extern struct cache_t *cache;
extern struct cache_syn_t *cache_syn;
extern struct state_syn_t *state_syn;
extern struct th_syn_t *th_syn;

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
