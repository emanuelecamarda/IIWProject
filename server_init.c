//
// Created by emanuele on 31/08/18.
//
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <signal.h>
#include <pthread.h>

#include "utils.h"

// set the default value for path
void set_default_path(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        error_found("Error in getcwd");
    }
    memset(LOG_PATH, 0, PATH_MAX);
    memset(IMG_PATH, 0, PATH_MAX);
    strcpy(LOG_PATH, cwd);
    strcpy(IMG_PATH, cwd);
}

void manage_signal(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        error_found("Error in sigaction!\n");
}

// manage option passed by users with command line
void manage_option(int argc, char **argv) {
    int i, flag_n = 0, flag_m = 0, c;
    char *e;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, USAGE_MSG, argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    while ((c = getopt(argc, argv, "p:l:i:n:m:r:c:")) != -1) {
        switch (c) {
            case 'p':
                errno = 0;
                int p_arg = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0' || p_arg > 65535 || p_arg < 1025) {
                    error_found("Error in option -p: port number must be a integer between 1025 and 65,535\n");
                }
                PORT = p_arg;
                break;

            case 'l':
                check_is_dir(optarg);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(LOG_PATH, optarg, strlen(optarg));
                } else {
                    strncpy(LOG_PATH, optarg, strlen(optarg) - 1);
                }

                if (LOG_PATH[strlen(LOG_PATH)] != '\0')
                    LOG_PATH[strlen(LOG_PATH)] = '\0';
                break;

            case 'i':
                check_is_dir(optarg);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(IMG_PATH, optarg, strlen(optarg));
                } else {
                    strncpy(IMG_PATH, optarg, strlen(optarg) - 1);
                }

                if (IMG_PATH[strlen(IMG_PATH)] != '\0')
                    IMG_PATH[strlen(IMG_PATH)] = '\0';
                break;

            case 'n':
                errno = 0;
                int min_th_num = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number of minimum thread!\n");
                if (min_th_num < 2)
                    error_found("Error: thread's minimum number must be >= 2!\n");
                MIN_TH_NUM = min_th_num;
                flag_n = 1;
                break;

            case 'm':
                errno = 0;
                int max_conn_num = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number of maximum thread!\n");
                if (max_conn_num < 1)
                    error_found("Error: thread's maximum number must be > 0!\n");
                MAX_CONN_NUM = max_conn_num;
                flag_m = 1;
                break;

            case 'r':
                errno = 0;
                int resize_perc = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number for resizing percentage!\n");
                if (resize_perc < 1 || resize_perc > 100)
                    error_found("Error: the resizing percentage must be an integer >= 1 and <= 100!\n");
                RESIZE_PERC = resize_perc;
                break;

            case 'c':
                errno = 0;
                int cache_size = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number for number of image in cache!\n");
                if (cache_size)
                    CACHE_SIZE = cache_size;
                else {
                    error_found("Error: number of image in cache must be an integer > 0 or -1 for infinite cache"
                                " size!\n");
                }
                break;

            case '?':
                error_found("Error: Invalid argument option!\n");
                break;

            default:
                error_found("Error in getopt!\n");
                break;
        }
    }
    if (flag_n && !flag_m && MAX_CONN_NUM < MIN_TH_NUM)
        MAX_CONN_NUM = 2 * MIN_TH_NUM;
    else if (!flag_n && flag_m && MIN_TH_NUM > MAX_CONN_NUM)
        MIN_TH_NUM = MAX_CONN_NUM / 2;
    if (MIN_TH_NUM > MAX_CONN_NUM)
        error_found("Error: maximum number of threads is lower then minimum number of threads!\n");
}

void struct_init(void) {
    pthread_mutex_t *mutex_cond, *mutex_cache, *mutex_th_num;
    pthread_cond_t *cond_th_init, *cond_max_conn;
    int i;

    mutex_cond = malloc(sizeof(pthread_mutex_t));
    mutex_cache = malloc(sizeof(pthread_mutex_t));
    mutex_th_num = malloc(sizeof(pthread_mutex_t));
    cond_th_init = malloc(sizeof(pthread_cond_t));
    cond_max_conn = malloc(sizeof(pthread_cond_t));
    SYN = malloc(sizeof(struct th_sync));

    if (pthread_mutex_init(mutex_cond, NULL) != 0 ||
        pthread_mutex_init(mutex_cache, NULL) != 0 ||
        pthread_mutex_init(mutex_th_num, NULL) != 0 ||
        pthread_cond_init(cond_th_init, NULL) != 0 ||
        pthread_cond_init(cond_max_conn, NULL) != 0)
        error_found("Error in pthread_mutex_init or pthread_cond_init!\n");

    SYN -> connections = SYN -> slot_c = SYN -> to_kill = SYN -> th_act = 0;
    SYN -> mutex_cond = mutex_cond;
    SYN -> mutex_cache = mutex_cache;
    SYN -> mutex_th_num = mutex_th_num;
    SYN -> cond_th_init = cond_th_init;
    SYN -> cond_max_conn = cond_max_conn;
    SYN -> th_act_thr = MIN_TH_NUM;
    SYN -> cache_hit_head = SYN -> cache_hit_tail = NULL;
    IMAGES = NULL;

    SYN -> clients = malloc(sizeof(int) * MAX_CONN_NUM);
    SYN -> new_c = malloc(sizeof(pthread_cond_t) * MAX_CONN_NUM);
    if (!SYN -> clients || !SYN -> new_c) {
        error_found("Error in malloc!\n");
    } else {
        memset(SYN -> clients, 0, sizeof(int) * MAX_CONN_NUM);
        memset(SYN->new_c, 0, sizeof(pthread_cond_t) * MAX_CONN_NUM);
    }
    // -1 := slot with thread initialized; -2 := empty slot.
    for (i = 0; i < MAX_CONN_NUM; ++i) {
        SYN -> clients[i] = -2;
        pthread_cond_t cond;
        if (pthread_cond_init(&cond, NULL) != 0)
            error_found("Error in pthread_cond_init\n");
        SYN -> new_c[i] = cond;
    }
}

void server_start(void) {
    int flag = 1;
    char s_server_start[STR_DIM], *k = "\t\tServer started at port:";
    struct sockaddr_in s_addr;

    if ((LISTEN_SD = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        error_found("Error in socket().\n");
    memset((void *) &s_addr, 0, sizeof(s_addr));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    s_addr.sin_port = htons((unsigned short) PORT);

    // To reuse a socket
    if (setsockopt(LISTEN_SD, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag)) != 0)
        error_found("Error in setsockopt().\n");

    errno = 0;
    if (bind(LISTEN_SD, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        if(errno == EADDRINUSE) {
            error_found("Error in bind: address already used!\n");
        }
        else {
            error_found("Error in bind: choose another socket!\n");
        }
    }

    if (listen(LISTEN_SD, MAX_CONN_NUM) != 0)
        error_found("Error in listen.\n");

    fprintf(stdout, "Server listen socket starting to listen at port: %d\n", PORT);

    // Update log
    LOG = open_file(LOG_PATH, "log");
    memset(s_server_start, 0, STR_DIM);
    sprintf(s_server_start, "%s %d\n", k, PORT);
    write_log(s_server_start);
}

void image_resize(void) {
    DIR *dir;
    struct dirent *ent;
    char *k, input[PATH_MAX], output[PATH_MAX], command[STR_DIM];
    // %s image's path; %d resizing percentage
    char *convert = "convert %s -resize %d%% %s;exit";
    struct image **i = &IMAGES;
    int first_image = 1;

    memset(input, 0, PATH_MAX);
    memset(output, 0, PATH_MAX);
    memset(command, 0, STR_DIM);

    // Create tmp folder for resized and cached images
    if (!mkdtemp(TMP_RESIZED_PATH) || !mkdtemp(TMP_CACHE_PATH))
        error_found("Error in mkdtmp!\n");

    errno = 0;
    dir = opendir(IMG_PATH);
    if (!dir) {
        if (errno == EACCES)
            error_found("Error: permission denied to open images' directory!\n");
        error_found("Error in opendir: cannot open images' directory!\n");
    }

    while ((ent = readdir(dir)) != NULL) {
        if (ent -> d_type != DT_REG) {
            continue;
        }

        if (strrchr(ent -> d_name, '~')) {
            fprintf(stderr, "File '%s' was skipped\n", ent -> d_name);
            continue;
        }

        else {
            if ((k = strrchr(ent -> d_name, '.'))) {
                if (strcmp(k, ".db") == 0) {
                    fprintf(stderr, "File '%s' was skipped\n", ent->d_name);
                    continue;
                }

                if (strcmp(k, ".c") == 0) {
                    fprintf(stderr, "File '%s' was skipped\n", ent->d_name);
                    continue;
                }

                if (strcmp(k, ".o") == 0) {
                    fprintf(stderr, "File '%s' was skipped\n", ent->d_name);
                    continue;
                }

                if (strcmp(k, ".h") == 0) {
                    fprintf(stderr, "File '%s' was skipped\n", ent->d_name);
                    continue;
                }

                if (strcmp(k, ".gif") != 0 && strcmp(k, ".GIF") != 0 &&
                    strcmp(k, ".jpg") != 0 && strcmp(k, ".JPG") != 0 &&
                    strcmp(k, ".png") != 0 && strcmp(k, ".PNG") != 0) {
                    fprintf(stderr, "Warning: file '%s' may have an unsupported format! The server support only file "
                                    ".gif, .jpg, .png\n", ent->d_name);
                    continue;
                }
            }
            else {
                fprintf(stderr, "File '%s' was skipped\n", ent -> d_name);
                continue;
            }
        }

        sprintf(input, "%s/%s", IMG_PATH, ent -> d_name);
        sprintf(output, "%s/%s", TMP_RESIZED_PATH, ent -> d_name);
        sprintf(command, convert, input, RESIZE_PERC, output);

        if (PRINT_DUMP)
            printf("Try to resize image %s.\n", ent -> d_name);

        if (system(command))
            error_found("Error in resizing images!\n");

        // populate the dynamic struct containing the resizing images
        alloc_res_img(i, output, first_image);
        i = &(*i) -> next_img;
        first_image = 0;
    }

    if (closedir(dir))
        error_found("Error in closedir!\n");

    fprintf(stdout, "Images resized in '%s' with percentage: %d%%\n", TMP_RESIZED_PATH, RESIZE_PERC);
}

void html_create(void) {
    printf("Sono qui 1\n");
    // create main page html
    size_t size = 4;
    char *html, *w, *q;
    struct image **i = &IMAGES;
    printf("Sono qui 2\n");
    // %s page's title; %s header; %s text.
    char *head = "<!DOCTYPE html><html><head><meta charset=\"utf-8\" /><title>%s</title>"
                            "<style type=\"text/css\"></style><script type=\"text/javascript\"></script></head>"
                            "<body background=\"\"><h1>%s</h1><br><br><h3>%s</h3><hr><br>";
    printf("Sono qui 3\n");
    // %s image's name; %s image's name; %s image's path; %s image's name.
    char *k = "<b>%s</b><br><br><a href=\"%s\"><img src=\"%s/%s\" height=\"130\" weight=\"100\"></a><br><br><br><br>";

    printf("Sono qui 4\n");
    html = malloc((size_t) size * STR_DIM * sizeof(char));
    if (!html)
        error_found("Error in malloc!\n");
    memset(html, 0, (size_t) size * STR_DIM * sizeof(char));

    printf("Sono qui 5\n");
    HTML[0] = malloc(sizeof(FILE) * 3);

    printf("Sono qui 6\n");
    HTML[0] = open_file(LOG_PATH, "main_page");
    printf("Sono qui 7\n");
    sprintf(html, head, "WebServerProject", "Welcome", "Select an image below");
    size_t len_h = strlen(html), new_len_h;
    size_t len = strlen(html);

    printf("Sono qui 8\n");
    while (!*i) {
        if (len + STR_DIM >= size * STR_DIM) {
            size++;
            html = realloc(html, (size_t) size * STR_DIM);
            if (!html)
                error_found("Error in realloc!\n");
            memset(html + len, 0, (size_t) size * STR_DIM - len);
        }

        printf("Sono qui 8.1\n");
        if (!(w = strrchr(TMP_RESIZED_PATH, '/')))
            error_found("Unexpected error creating HTML root file\n");
        printf("Sono qui 8.2\n");
        w++;
        printf("Sono qui 8.3\n");
        q = html + len;
        sprintf(q, k, &(*i) -> name, &(*i) -> name, w, &(*i) -> name);
        printf("Sono qui 8.4\n");
        *i = &(*i) -> next_img;
    }

    printf("Sono qui 9\n");
    new_len_h = strlen(html);
    if (len_h == new_len_h)
        error_found("Error: there aren't images to resize!\n");

    head = "</body></html>";
    if (new_len_h + STR_DIM > size * STR_DIM) {
        size++;
        html = realloc(html, (size_t) size * STR_DIM);
        if (!html)
            error_found("Error in realloc!\n");
        memset(html + new_len_h, 0, (size_t) size * STR_DIM - new_len_h);
    }
    k = html;
    k += strlen(html);
    strcpy(k, head);
    writef(html, HTML[0]);
    if (fclose(HTML[0]) != 0) {
        error_found("Error in fclose!");
    }

    printf("Sono qui 10\n");

    // create error page html
    // %s page's title; %s page's header; %s errror's description
    head = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>%s</title>"
           "</head><body><h1>%s</h1><p>%s</p></body></html>\0";
    len = strlen(head) + 2 * STR_DIM * sizeof(char);

    HTML[1] = open_file(LOG_PATH, "not_found");
    HTML[2] = open_file(LOG_PATH, "bad_request");

    printf("Sono qui 11\n");
    char *description1 = malloc(len);
    char *description2 = malloc(len);
    if (!description1 || !description2)
        error_found("Error in malloc!\n");
    memset(description1, 0, len);
    memset(description2, 0, len);
    sprintf(description1, head, "404 Not Found", "404 Not Found", "Sorry, page not found on this server...");
    sprintf(description2, head, "400 Bad Request", "Bad Request", "Sorry, I don't understand your request...");
    writef(description1, HTML[1]);
    writef(description2, HTML[2]);
    if (fclose(HTML[1]) != 0) {
        error_found("Error in fclose!");
    }
    if (fclose(HTML[2]) != 0) {
        error_found("Error in fclose!");
    }
}