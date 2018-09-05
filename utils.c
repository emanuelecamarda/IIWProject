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

#include "utils.h"

int PORT = 11111;
char *USAGE_MSG =
        "Usage: %s [option]\n\n"
        "\tThis is the list of the possible option:"
        "\t\t-p port number, default value: 11111.\n"
        "\t\t-i image directory's path, default: current directory.\n"
        "\t\t-l log file directory's path, default: current directory.\n"
        "\t\t-n initial thread number, default value: 10.\n"
        "\t\t-m maximum connection number, default value: 100.\n"
        "\t\t-r resize images' percentage, default value: 50.\n"
        "\t\t-c maximum number of image in cache, default value: -1 (infinite).\n"
        "\t\t-h help\n\n";
char *LOG_PATH;
char *IMG_PATH;
char TMP_RESIZED_PATH[PATH_MAX] = "/tmp/RESIZED.XXXXXX";
char TMP_CACHE_PATH[PATH_MAX] = "/tmp/CACHE.XXXXXX";
int MIN_TH_NUM = 10;
int MAX_CONN_NUM = 100;
int RESIZE_PERC = 50;
int CACHE_SIZE = -1;
int LISTEN_SD;
FILE *LOG;
FILE HTML[3];

struct image *IMAGES;
struct cache *CACHE;
struct th_sync *SYN;

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
    free(SYN.clients);
    free(SYN.new_c);
    if (CACHE_SIZE >= 0 && SYN.cache_hit_head &&SYN.cache_hit_tail) {
        struct cache_hit *to_be_removed;
        while (SYN.cache_hit_tail) {
            to_be_removed = SYN.cache_hit_tail;
            SYN.cache_hit_tail = SYN.cache_hit_tail -> next_hit;
            free(to_be_removed);
        }
    }
    rm_dir(tmp_resized);
    rm_dir(tmp_cache);
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
        exit_on_error("Unexpected error in remove directory!\n");

    // case tmp dir not create
    verify = strrchr(directory, '.') + 1;
    if (!strncmp(verify, "XXXXXX", 7))
        return;

    errno = 0;
    dir = opendir(directory);
    if (!dir) {
        if (errno == EACCES)
            exit_on_error("Permission denied to open directory!\n");
        exit_on_error("Error in opendir in remove directory!\n");
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
        exit_on_error("Error in closedir on remove directory!\n");

    errno = 0;
    if (rmdir(directory)) {
        switch (errno) {
            case EBUSY:
                sprintf(e, "Directory %s not removed: resource busy\n", directory);
                exit_on_error(e);

            case ENOMEM:
                sprintf(e, "Directory %s not removed: Insufficient kernel memory\n", directory);
                exit_on_error(e);

            case EROFS:
                sprintf(e, "Directory %s not removed: Pathname refers to a directory on a read-only file system\n", path);
                exit_on_error(e);

            case ENOTEMPTY:
                sprintf(e, "Directory %s not removed: Directory not empty!\n", path);
                exit_on_error(e);

            default:
                exit_on_error("Error in rmdir\n");
        }
    }
}

// Used to remove file from file system
void rm_link(char *path) {
    char e[STR_DIM];
    memset(e, 0, STR_DIM);
    if (unlink(path)) {
        errno = 0;
        switch (errno) {
            case EBUSY:
                sprintf(e, "File %s cannot be unlinked: It is being use by the system\n", path);
                exit_on_error(e);

            case EIO:
                sprintf(e, "File %s cannot be unlinked: An I/O error occurred\n", path);
                exit_on_error(e);

            case ENAMETOOLONG:
                sprintf(e, "File %s cannot be unlinked: Pathname was too long\n", path);
                exit_on_error(e);

            case ENOMEM:
                sprintf(e, "File %s cannot be unlinked: Insufficient kernel memory was available\n", path);
                exit_on_error(e);

            case EPERM:
                sprintf(e, "File %s cannot be unlinked: The file system does not allow unlinking of files\n",path);
                exit_on_error(e);

            case EROFS:
                sprintf(e, "File %s cannot be unlinked: Pathname refers to a file on a read-only file system\n", path);
                exit_on_error(e);

            default:
                sprintf(e, "File %s cannot be unlinked: Error in unlink\n", path);
                exit_on_error(e);
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
    char s[strlen(path) + 4 + 1];
    memset(s, 0, strlen(path) + 1 + strlen(file_name) + 1);
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

void alloc_res_img(struct image **i, char *path, int first_image) {
    char new_path[PATH_MAX], *img_name;
    struct image *new_img;
    struct stat buf;

    img_name = strrchr(path, '/');
    if (!img_name) {
        error_found("alloc_r_img: Error analyzing file");
    }
    else {
        strcpy(new_img->name, img_name++);
    }

    new_img = malloc(sizeof(struct image));
    if (!new_img)
        error_found("Error in malloc\n");

    memset(new_path, 0, PATH_MAX);
    memset(new_img, 0, sizeof(struct image));

    check_is_file(path, &buf);

    new_img->size_r = (size_t) buf.st_size;
    new_img->img_c = NULL;

    if (first_image) {
        new_img->next_img = *i;
        *i = new_img;
    } else {
        new_img->next_img = (*i)->next_img;
        (*i)->next_img = new_img;
    }
}