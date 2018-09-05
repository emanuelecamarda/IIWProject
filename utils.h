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

struct image {
    // Name of current image
    char name[NAME_MAX];
    // Memory mapped of resized image
    size_t size_r;
    struct cache *img_c;
    struct image *next_img;
};

struct cache {
    // quality factor
    int q;
    char img_q[NAME_MAX];
    size_t size_q;
    struct cache *next_img_c;
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
    int *clients;
    volatile int slot_c,
            connections,
            th_act,
            th_act_thr,
            to_kill;
    // To manage thread's number and connections
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

extern int PORT;
extern char *USAGE_MSG;
extern char *LOG_PATH;
extern char *IMG_PATH;
extern char TMP_RESIZED_PATH[PATH_MAX];
extern char TMP_CACHE_PATH[PATH_MAX];
extern int MIN_TH_NUM;
extern int MAX_CONN_NUM;
extern int RESIZE_PERC;
extern int CACHE_SIZE;
extern int LISTEN_SD;
extern FILE *LOG;
extern FILE HTML[3];

extern struct image *IMAGES;
extern struct cache *CACHE;
extern struct th_sync *SYN;

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
void alloc_res_img(struct image **i, char *path, int first_image);

#endif //PROGETTO_UTILS_H
