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
#include "threads_work.h"

// set the default value for path
void set_default_path(void) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        error_found("Error in getcwd!");
    }
    memset(LOG_PATH, 0, PATH_MAX);
    memset(IMG_PATH, 0, PATH_MAX);
    strcpy(LOG_PATH, cwd);
    strcpy(IMG_PATH, cwd);
}

// Ignore signal SIGPIPE
void manage_signal(void) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_handler = SIG_IGN;
    if (sigaction(SIGPIPE, &sa, NULL) == -1)
        error_found("Error in sigaction!\n");
}

// manage option passed by users with command line
void manage_option(int argc, char **argv) {
    int i, flag_n = 0, flag_m = 0, flag_u = 0, flag_d = 0, c;
    char *e;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, USAGE_MSG, argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    while ((c = getopt(argc, argv, "p:l:i:n:m:r:c:u:d:")) != -1) {
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
                memset(LOG_PATH, (int) '\0', PATH_MAX);

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
                memset(IMG_PATH, (int) '\0', PATH_MAX);

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
                printf("optarg di 'r' = %s\n", optarg);
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
                    cache_space = cache_size;
                else {
                    error_found("Error: number of image in cache must be an integer > 0 or -1 for infinite cache"
                                " size!\n");
                }
                break;

            case 'u':
                errno = 0;
                int scaling_up = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number for scaling up!\n");
                if (scaling_up <= 0 || scaling_up >= 100)
                    error_found("Error: the scaling up must be an integer > 1 and < 100!\n");
                TH_SCALING_UP = scaling_up;
                flag_u = 1;
                break;

            case 'd':
                errno = 0;
                int scaling_down = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Error in strtol: Invalid number for scaling down!\n");
                if (scaling_down <= 0 || scaling_down >= 100)
                    error_found("Error: the scaling down must be an integer > 1 and < 100!\n");
                TH_SCALING_DOWN = scaling_down;
                flag_d = 1;
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

    if (flag_d && !flag_u && TH_SCALING_UP < TH_SCALING_DOWN)
        TH_SCALING_UP = 2 * TH_SCALING_DOWN;
    else if (!flag_d && flag_u && TH_SCALING_DOWN > TH_SCALING_UP)
        TH_SCALING_DOWN = TH_SCALING_UP / 2;
    if (TH_SCALING_DOWN > TH_SCALING_UP)
        error_found("Error: scaling up is lower then scaling down!\n");
}

// Initialize all struct needed to server
void struct_init(void) {
    pthread_mutex_t *cache_syn_mtx, *state_syn_mtx, *th_syn_mtx;
    pthread_cond_t *cache_syn_cond, *state_syn_cond;
    int i;

    cache_syn_mtx = malloc(sizeof(pthread_mutex_t));
    if (!cache_syn_mtx) {
        error_found("Error in malloc!\n");
    }
    cache_syn_cond = malloc(sizeof(pthread_cond_t));
    if (!cache_syn_cond) {
        error_found("Error in malloc!\n");
    }

    state_syn_mtx = malloc(sizeof(pthread_mutex_t));
    if (!state_syn_mtx) {
        error_found("Error in malloc!\n");
    }
    state_syn_cond = malloc(sizeof(pthread_cond_t));
    if (!state_syn_cond) {
        error_found("Error in malloc!\n");
    }

    th_syn_mtx = malloc(sizeof(pthread_mutex_t));
    if (!th_syn_mtx) {
        error_found("Error in malloc!\n");
    }

    cache_syn = malloc(sizeof(struct cache_syn_t));
    if (!cache_syn) {
        error_found("Error in malloc!\n");
    }
    state_syn = malloc(sizeof(struct state_syn_t));
    if (!state_syn) {
        error_found("Error in malloc!\n");
    }
    th_syn = malloc(sizeof(struct th_syn_t));
    if (!th_syn) {
        error_found("Error in malloc!\n");
    }

    th_syn -> cond = malloc(sizeof(pthread_cond_t) * MAX_CONN_NUM);
    if (!th_syn -> cond) {
        error_found("Error in malloc!\n");
    }


    if (pthread_mutex_init(cache_syn_mtx, NULL) != 0 ||
        pthread_mutex_init(state_syn_mtx, NULL) != 0 ||
        pthread_mutex_init(th_syn_mtx, NULL) != 0 ||
        pthread_cond_init(cache_syn_cond, NULL) != 0 ||
        pthread_cond_init(state_syn_cond, NULL) != 0)
        error_found("Error in pthread_mutex_init or pthread_cond_init!\n");

    memset(th_syn -> cond, (int) '\0', sizeof(pthread_cond_t) * MAX_CONN_NUM);

    cache_syn -> mtx = cache_syn_mtx;
    cache_syn -> cond = cache_syn_cond;
    cache_syn -> cache_hit_head = cache_syn -> cache_hit_tail = NULL;
    state_syn -> mtx = state_syn_mtx;
    state_syn -> cond = state_syn_cond;
    th_syn -> mtx = th_syn_mtx;
    state_syn -> conn_num = state_syn -> init_th_num = th_syn -> accept = 0;
    IMAGES = NULL;

    th_syn -> cl_addr = malloc(sizeof(struct sockaddr_in));
    if (!th_syn -> cl_addr){
        fprintf(stderr, "Error in malloc()!");
    }

    th_syn -> clients = malloc(sizeof(int) * MAX_CONN_NUM);
    if (!th_syn -> clients) {
        error_found("Error in malloc!\n");
    } else {
        memset(th_syn -> clients, 0, sizeof(int) * MAX_CONN_NUM);
    }

    // -1 := slot with thread initialized; -2 := empty slot
    for (i = 0; i < MAX_CONN_NUM; ++i) {
        th_syn -> clients[i] = -2;
        pthread_cond_t cond;
        if (pthread_cond_init(&cond, NULL) != 0)
            error_found("Error in pthread_cond_init!\n");
        th_syn -> cond[i] = cond;
    }
}

// Do the socket(),  bind() and listen() operations for server
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

// Do the resizing of all images save in the struct IMAGES
void image_resize(void) {
    DIR *dir;
    struct dirent *ent;
    char *k, input[PATH_MAX], output[PATH_MAX], command[STR_DIM];
    // %s image's path; %d resizing percentage
    char *convert = "convert %s -resize %d%% %s;exit";
    struct image_t **i = &IMAGES;

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
                    strcmp(k, ".png") != 0 && strcmp(k, ".PNG") != 0 &&
                    strcmp(k, ".jpeg") != 0 && strcmp(k, ".JPEG") != 0) {
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

        printf("Try to resize image %s\n", ent -> d_name);

        if (system(command))
            error_found("Error in resizing images!\n");

        // populate the dynamic struct containing the resizing images
        alloc_res_img(i, output);
        i = &(*i) -> next_img;
    }

    if (closedir(dir))
        error_found("Error in closedir!\n");

    fprintf(stdout, "Images resized in '%s' with percentage: %d%%\n", TMP_RESIZED_PATH, RESIZE_PERC);
}

// create the html main_page and error response
void html_create(void) {
    // create main page html
    size_t size = 4;
    char *html, *w, *q;
    struct image_t *i = IMAGES;
    // %s page's title; %s header; %s text.
    char *head = "<!DOCTYPE html><html><head><meta charset=\"utf-8\" /><title>%s</title>"
                            "<style type=\"text/css\"></style><script type=\"text/javascript\"></script></head>"
                            "<body background=\"\"><h1>%s</h1><br><br><h3>%s</h3><hr><br>";
    // %s image's name; %s image's name; %s image's path; %s image's name.
    char *k = "<b>%s</b><br><br><a href=\"%s\"><img src=\"%s/%s\" height=\"130\" weight=\"100\"></a><br><br><br><br>";

    html = malloc((size_t) size * STR_DIM * sizeof(char));
    if (!html)
        error_found("Error in malloc!\n");
    memset(html, 0, (size_t) size * STR_DIM * sizeof(char));

    sprintf(html, head, "IIWProject", "IIWProject", "Select an image below");
    size_t len_h = strlen(html), new_len_h;

    while (1) {
        if (!i)
            break;

        size_t len = strlen(html);
        if (len + STR_DIM >= size * STR_DIM) {
            size++;
            html = realloc(html, (size_t) size * STR_DIM);
            if (!html)
                error_found("Error in realloc!\n");
            memset(html + len, (int) '\0', STR_DIM);
        }

        if (!(w = strrchr(TMP_RESIZED_PATH, '/')))
            error_found("Unexpected error creating HTML root file\n");
        w++;
        q = html + len;
        sprintf(q, k, i -> name, i -> name, w, i -> name);
        i = i -> next_img;
    }

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

    HTML[0] = malloc(sizeof(char) * strlen(html));
    memset(HTML[0], (int) '\0', strlen(html));
    sprintf(HTML[0], html, strlen(html));

    // create error page html
    // %s page's title; %s page's header; %s errror's description
    head = "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 2.0//EN\"><html><head><title>%s</title>"
           "</head><body><h1>%s</h1><p>%s</p></body></html>\0";
    size_t  len = strlen(head) + 3 * STR_DIM * sizeof(char);

    char *description1 = malloc(len);
    char *description2 = malloc(len);
    if (!description1 || !description2)
        error_found("Error in malloc!\n");
    memset(description1, 0, len);
    memset(description2, 0, len);
    sprintf(description1, head, "404 Not Found", "404 Not Found", "Sorry, page not found on this server...");
    sprintf(description2, head, "400 Bad Request", "Bad Request", "Sorry, I don't understand your request...");

    HTML[1] = malloc(sizeof(char) * strlen(description1));
    HTML[2] = malloc(sizeof(char) * strlen(description2));
    memset(HTML[1], (int) '\0', strlen(description1));
    memset(HTML[2], (int) '\0', strlen(description2));
    sprintf(HTML[1], description1, strlen(description1));
    sprintf(HTML[2], description2, strlen(description2));
}

// Main thread work: accept() and pass to a worker a new connection
void main_th_work(void) {
    int conn_sd, i = 0, j = 1;
    struct sockaddr_in cl_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    fprintf(stdout, "\n\nWaiting for incoming connection...\n");

    // Accept connections
    while (1) {
        // check MAX_CONN_NUM
        lock(state_syn -> mtx);
        if (PRINT_DUMP)
            printf("Main thread lock state_syn\n");
        {
            while (state_syn -> conn_num >= MAX_CONN_NUM) {
                if (PRINT_DUMP)
                    printf("Main thread wait on state_syn cond\n");
                wait_cond(state_syn -> cond, state_syn -> mtx);
            }
        } unlock(state_syn -> mtx);
        if (PRINT_DUMP)
            printf("Main thread unlock state_syn\n");
        memset(&cl_addr, (int) '\0', addr_size);
        errno = 0;
        conn_sd = accept(LISTEN_SD, (struct sockaddr *) &cl_addr, &addr_size);
        lock(th_syn -> mtx);
        if (PRINT_DUMP)
            printf("Main thread lock th_syn\n");
        {
            if (conn_sd == -1) {
                switch (errno) {
                    case ECONNABORTED:
                        fprintf(stderr, "Error in accept(): connection has been aborted!\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    case ENOBUFS:
                        error_found("Error in accept(): not enough free memory!\n");
                        break;

                    case ENOMEM:
                        error_found("Error in accept(): not enough free memory\n");
                        break;

                    case EMFILE:
                        fprintf(stderr, "Error in accept(): too many open files!\n");
                        lock(state_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread lock state_syn\n");
                        {
                            int old_conn_num = state_syn->conn_num;
                            while (old_conn_num >= state_syn->conn_num) {
                                if (PRINT_DUMP)
                                    printf("Main thhread wait on state_syn cond\n");
                                wait_cond(state_syn->cond, state_syn->mtx);
                            }
                        } unlock(state_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock state_syn\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    case EPROTO:
                        fprintf(stderr, "Error in accept(): protocol error!\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    case EPERM:
                        fprintf(stderr, "Error in accept(): firewall rules forbid connection!\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    case ETIMEDOUT:
                        fprintf(stderr, "Error in accept(): timeout occure!\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    case EBADF:
                        fprintf(stderr, "Error in accept(): bad file number!\n");
                        unlock(th_syn->mtx);
                        if (PRINT_DUMP)
                            printf("Main thread unlock th_syn\n");
                        continue;

                    default:
                        error_found("Error in accept()!\n");
                        break;
                }
            }

            j = 1;
            while (th_syn -> clients[i] != -1) {
                if (j > MAX_CONN_NUM) {
                    j = -1;
                    break;
                }
                i = (i + 1) % MAX_CONN_NUM;
                j++;
            }

            // MAX_CONN_NUM
            if (j == -1) {
                unlock(th_syn -> mtx);
                if (PRINT_DUMP)
                    printf("Main thread unlock th_syn\n");
                continue;
            }

            th_syn -> clients[i] = conn_sd;
            th_syn -> accept = 0;
            memset(th_syn -> cl_addr, (int) '\0', addr_size);
            memcpy(th_syn -> cl_addr, &cl_addr, addr_size);
            if (PRINT_DUMP)
                printf("Main thread signal th_syn cond[%d]\n", i);
            signal_cond(th_syn -> cond + i);
            while (!th_syn -> accept) {
                if (PRINT_DUMP)
                    printf("Main thread wait on th_syn cond[%d]\n", i);
                wait_cond(&th_syn->cond[i], th_syn -> mtx);
            }
            i = (i + 1) % MAX_CONN_NUM;
        } unlock(th_syn -> mtx);
        if (PRINT_DUMP)
            printf("Main thread unlock th_syn\n");
    }
}