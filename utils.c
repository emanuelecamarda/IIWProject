//
// Created by emanuele on 31/08/18.
//
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include <fcntl.h>

#include "utils.h"

int PORT = 11111;
char *USAGE_MSG =
        "Usage: %s [option]\n\n"
        "This is the list of the possible option:\n"
        "\t-p port number, default value: 11111.\n"
        "\t-i image directory's path, default: current directory.\n"
        "\t-l log file directory's path, default: current directory.\n"
        "\t-n initial thread number, default value: 10.\n"
        "\t-m maximum connection number, default value: 800.\n"
        "\t-r resize images' percentage, default value: 50.\n"
        "\t-c maximum number of image in cache, default value: -1 (infinite).\n"
        "\t-u scaling up factor of threads, default value: 75\n"
        "\t-d scaling down factor of threads, default value: 25\n"
        "\t-h help\n\n";
char *USER_OPT =
        "Press q or Q to close the server\n"
        "Press s or S to show the server's state\n";
char LOG_PATH[PATH_MAX];
char IMG_PATH[PATH_MAX];
char TMP_RESIZED_PATH[PATH_MAX] = "/tmp/RESIZED.XXXXXX";
char TMP_CACHE_PATH[PATH_MAX] = "/tmp/CACHE.XXXXXX";
int MIN_TH_NUM = 10;
int MAX_CONN_NUM = 800;
int RESIZE_PERC = 50;
int cache_space = -1;
int LISTEN_SD;
FILE *LOG;
char *HTML[3];
int TH_SCALING_UP = 75;
int TH_SCALING_DOWN = 25;

struct image_t *IMAGES;
struct cache_t *cache;
struct cache_syn_t *cache_syn;
struct state_syn_t *state_syn;
struct th_syn_t *th_syn;
struct accept_conn_syn_t *accept_conn_syn;

// write on stderr and on log file the error that occurs and exit with failure
void error_found(char *msg) {
    fprintf(stderr, "%s", msg);
    if (LOG) {
        writef(msg, LOG);
    }
    free_mem();
    exit(EXIT_FAILURE);
}

// write the string s on a file
void writef(char *s, FILE *file) {
    size_t n;
    size_t len = strlen(s);

    while ((n = fwrite(s, sizeof(char), len, file)) < len) {
        if (ferror(file)) {
            fprintf(stderr, "Error in fwrite\n");
            exit(EXIT_FAILURE);
        }
        len -= n;
        s += n;
    }
}

// dealloc all memory used by server
void free_mem() {
    char s[STR_DIM];
    memset(s, (int) '\0', STR_DIM);

    free(th_syn -> clients);
    free(th_syn -> mtx);
    free(th_syn -> cond);
    free(state_syn -> mtx);
    free(state_syn -> cond);
    free(cache_syn -> mtx);
    free(cache_syn -> cond);
    free(accept_conn_syn -> mtx);
    free(accept_conn_syn -> cond);
    free(accept_conn_syn -> cl_addr);
    if (cache_space >= 0 && cache_syn -> cache_hit_head && cache_syn -> cache_hit_tail) {
        struct cache_hit *to_be_removed;
        while (cache_syn -> cache_hit_tail) {
            to_be_removed = cache_syn -> cache_hit_tail;
            cache_syn -> cache_hit_tail = cache_syn -> cache_hit_tail -> next_hit;
            free(to_be_removed);
        }
    }
    free(th_syn);
    free(state_syn);
    free(cache_syn);
    free(accept_conn_syn);
    rm_dir(TMP_RESIZED_PATH);
    rm_dir(TMP_CACHE_PATH);
}

// Used to remove directory from file system
void rm_dir(char *directory) {
    DIR *dir;
    struct dirent *ent;
    char *verify, e[STR_DIM];
    memset(e, 0, STR_DIM);

    fprintf(stdout, "Remove '%s'\n", directory);
    verify = strrchr(directory, '/') + 1;
    if (!verify)
        error_found("Unexpected error in remove directory!\n");

    // case tmp dir not create
    verify = strrchr(directory, '.') + 1;
    if (!strncmp(verify, "XXXXXX", 7))
        return;

    errno = 0;
    dir = opendir(directory);
    if (!dir) {
        if (errno == EACCES)
            error_found("Permission denied to open directory!\n");
        error_found("Error in opendir in remove directory!\n");
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent -> d_type == DT_REG) {
            char buf[STR_DIM];
            memset(buf, 0, STR_DIM);
            sprintf(buf, "%s/%s", directory, ent -> d_name);
            rm_link(buf);
        }
    }

    if (closedir(dir))
        error_found("Error in closedir on remove directory!\n");

    errno = 0;
    if (rmdir(directory)) {
        switch (errno) {
            case EBUSY:
                sprintf(e, "Directory %s not removed: resource busy\n", directory);
                error_found(e);
                break;

            case ENOMEM:
                sprintf(e, "Directory %s not removed: Insufficient kernel memory\n", directory);
                error_found(e);
                break;

            case EROFS:
                sprintf(e, "Directory %s not removed: Pathname refers to a directory on a read-only file system\n",
                        directory);
                error_found(e);
                break;

            case ENOTEMPTY:
                sprintf(e, "Directory %s not removed: Directory not empty!\n", directory);
                error_found(e);
                break;

            default:
                error_found("Error in rmdir\n");
                break;
        }
    }
}

// Used to remove file from file system
void rm_link(char *path) {
    char e[STR_DIM];
    memset(e, 0, STR_DIM);

    fprintf(stdout, "Remove '%s'\n", path);
    if (unlink(path)) {
        errno = 0;
        switch (errno) {
            case EBUSY:
                sprintf(e, "File %s cannot be unlinked: It is being use by the system\n", path);
                error_found(e);
                break;

            case EIO:
                sprintf(e, "File %s cannot be unlinked: An I/O error occurred\n", path);
                error_found(e);
                break;

            case ENAMETOOLONG:
                sprintf(e, "File %s cannot be unlinked: Pathname was too long\n", path);
                error_found(e);
                break;

            case ENOMEM:
                sprintf(e, "File %s cannot be unlinked: Insufficient kernel memory was available\n", path);
                error_found(e);
                break;

            case EPERM:
                sprintf(e, "File %s cannot be unlinked: The file system does not allow unlinking of files\n",path);
                error_found(e);
                break;

            case EROFS:
                sprintf(e, "File %s cannot be unlinked: Pathname refers to a file on a read-only file system\n", path);
                error_found(e);
                break;

            default:
                sprintf(e, "File %s cannot be unlinked: Error in unlink\n", path);
                error_found(e);
                break;
        }
    }
}

// check if the path is a dir. If is not a dir, process exit with error
void check_is_dir(char *path) {
    struct stat buf;
    memset(&buf, 0, sizeof(struct stat));
    errno = 0;
    if (stat(path, &buf) != 0) {
        if (errno == ENAMETOOLONG)
            error_found("Error in check_is_dir: name is too long!\n");
        error_found("Error in check_is_dir!\n");
    }
    if (!S_ISDIR((buf).st_mode)) {
        error_found("Error in check_is_dir!\n");
    }
}

// check if the path is a regular file. If is not a file, process exit with error
void check_is_file(char *path, struct stat *buf) {
    memset(buf, 0, sizeof(struct stat));
    errno = 0;
    if (stat(path, buf) != 0) {
        if (errno == ENAMETOOLONG)
            error_found("Error in check_is_file: name is too long!\n");
        error_found("Error in check_is_file!\n");
    }
    if (!S_ISREG((*buf).st_mode)) {
        error_found("Error in check_is_file!\n");
    }
}

// Open and return a file from fs
FILE *open_file(char *path, char *file_name) {
    errno = 0;
    char s[strlen(path) + 1 + strlen(file_name)];
    memset(s, 0, (size_t) strlen(path) + 1 + strlen(file_name));
    sprintf(s, "%s/%s", path, file_name);
    FILE *f = fopen(s, "a");
    if (!f) {
        if (errno == EACCES) {
            error_found("Error in fopen: missing permission!");
        }
        error_found("Error in fopen.\n");
    }
    fprintf(stdout, "File %s correctly created in: '%s'\n", file_name, path);
    return f;
}

// Write string s on log file
void write_log(char *s) {
    char *t = get_time();
    writef(t, LOG);
    writef(s, LOG);
    free(t);
}

// Used to get current time
char *get_time(void) {
    time_t now = time(NULL);
    char *s = malloc(sizeof(char) * STR_DIM);
    if (!s)
        error_found("Error in malloc!\n");
    memset(s, 0, sizeof(char) * STR_DIM);
    strcpy(s, ctime(&now));
    if (!s)
        error_found("Error in ctime!\n");
    if (s[strlen(s) - 1] == '\n')
        s[strlen(s) - 1] = '\0';
    return s;
}

void alloc_res_img(struct image_t **i, char *path) {
    char new_path[PATH_MAX], *img_name;
    struct image_t *new_img;
    struct stat buf;

    new_img = malloc(sizeof(struct image_t));
    if (!new_img)
        error_found("Error in malloc\n");
    memset(new_img, 0, sizeof(struct image_t));

    img_name = strrchr(path, '/');
    if (!img_name) {
        error_found("alloc_r_img: Error analyzing file");
    }
    else {
        strcpy(new_img -> name, ++img_name);
    }
    memset(new_path, 0, PATH_MAX);

    check_is_file(path, &buf);

    new_img -> size_r = (size_t) buf.st_size;
    new_img -> img_c = NULL;

    if (!*i) {
        new_img -> next_img = *i;
        *i = new_img;
    } else {
        new_img -> next_img = (*i) -> next_img;
        (*i) -> next_img = new_img;
    }
}

// Used to get image from file system
char *get_img(char *name, size_t img_dim, char *directory) {
    ssize_t left = 0;
    int fd;
    char *buf;
    char path[strlen(name) + strlen(directory) + 1];
    memset(path, (int) '\0', strlen(name) + strlen(directory) + 1);
    sprintf(path, "%s/%s", directory, name);
    if (path[strlen(path)] != '\0')
        path[strlen(path)] = '\0';

    errno = 0;
    if ((fd = open(path, O_RDONLY)) == -1) {
        switch (errno) {
            case EACCES:
                fprintf(stderr, "Error in get_img: Permission denied!\n");
                break;

            case EISDIR:
                fprintf(stderr, "Error in get_img: '%s' is a directory!\n", name);
                break;

            case ENFILE:
                fprintf(stderr, "Error in get_img: The maximum allowable number of files is currently open in "
                                "the system!\n");
                break;

            case EMFILE:
                fprintf(stderr, "Error in get_img: File descriptors are currently open in the calling process!\n");
                break;

            default:
                fprintf(stderr, "Error in get_img!\n");
                break;
        }
        return NULL;
    }

    errno = 0;
    if (!(buf = malloc(img_dim))) {
        fprintf(stderr, "Error in malloc:\t\terrno: %d\t\timg_dim: %d\n", errno, (int) img_dim);
        return buf;
    } else {
        memset(buf, (int) '\0', img_dim);
    }
    while ((left = read(fd, buf + left, img_dim)))
        img_dim -= left;
    if (close(fd)) {
        fprintf(stderr, "Error in close():\t\tFile Descriptor: %d\n", fd);
    }
    return buf;
}