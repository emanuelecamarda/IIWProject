//
// Created by Emanuele on 08/09/2018.
//
#include <sys/socket.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>

#include "utils.h"
#include "server_http.h"
#include "threads_work.h"

void analyze_http_request(int conn_sd, struct sockaddr_in cl_addr) {
    char http_req[STR_DIM * STR_DIM];
    char *line_req[7];
    ssize_t tmp;
    int i;
    struct timeval tv;
    tv.tv_sec = 10;
    tv.tv_usec = 0;

    if (setsockopt(conn_sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval)) < 0)
        fprintf(stderr, "Error in setsockopt: cannot set timeval struct!\n");

    do {
        memset(http_req, (int) '\0', 5 * STR_DIM);
        for (i = 0; i < 7; ++i)
            line_req[i] = NULL;

        errno = 0;
        tmp = recv(conn_sd, http_req, 5 * STR_DIM, 0);

        if (tmp == -1) {
            switch (errno) {
                case EFAULT:
                    fprintf(stderr, "Error in recv: the receive  buffer  pointer(s)  point  outside  the  process's "
                                    "address space!");
                    break;

                case EBADF:
                    fprintf(stderr, "Error in recv: invalid socket descriptor: %d\n", conn_sd);
                    break;

                case ECONNREFUSED:
                    fprintf(stderr, "Error in recv: remote host refused to allow the network connection!\n");
                    break;

                case EINVAL:
                    fprintf(stderr, "Error in recv: invalid argument passed!\n");
                    break;

                case EINTR:
                    fprintf(stderr, "Error in recv: timeout receiving from socket!\n");
                    break;

                case EWOULDBLOCK:
                    fprintf(stderr, "Error in recv: timeout receiving from socket!\n");
                    break;

                default:
                    fprintf(stderr, "Error in recv: error while receiving data from client!\n");
                    break;
            }
            break;
        } else if (tmp == 0) {
            fprintf(stderr, "Error in recv: client disconnected\n");
            break;
        } else {
            split_http_request(http_req, line_req);

            char log[STR_DIM];
            memset(log, (int) '\0', STR_DIM);
            sprintf(log, "\tClient:\t%s\tRequest: '%s %s %s'\n",
                    inet_ntoa(cl_addr.sin_addr), line_req[0], line_req[1], line_req[2]);
            write_log(log);

            if (http_response(conn_sd, line_req))
                break;
        }
    } while (line_req[3] && !strncmp(line_req[3], "keep-alive", 10));
}

// Used to split HTTP message
void split_http_request(char *s, char **buf) {
    char *req_header[4], *k;
    req_header[0] = "Connection: ";
    req_header[1] = "User-Agent: ";
    req_header[2] = "Accept: ";
    req_header[3] = "Cache-Control: ";
    // HTTP message type
    buf[0] = strtok(s, " ");
    // Requested object
    buf[1] = strtok(NULL, " ");
    // HTTP version
    buf[2] = strtok(NULL, "\n");
    if (buf[2]) {
        if (buf[2][strlen(buf[2]) - 1] == '\r')
            buf[2][strlen(buf[2]) - 1] = '\0';
    }

    while ((k = strtok(NULL, "\n"))) {
        // Connection header
        if (!strncmp(k, req_header[0], strlen(req_header[0]))) {
            buf[3] = k + strlen(req_header[0]);
            if (buf[3][strlen(buf[3]) - 1] == '\r')
                buf[3][strlen(buf[3]) - 1] = '\0';
        }
        // User-Agent header
        else if (!strncmp(k, req_header[1], strlen(req_header[1]))) {
            buf[4] = k + strlen(req_header[1]);
            if (buf[4][strlen(buf[4]) - 1] == '\r')
                buf[4][strlen(buf[4]) - 1] = '\0';
        }
        // Accept header
        else if (!strncmp(k, req_header[2], strlen(req_header[2]))) {
            buf[5] = k + strlen(req_header[2]);
            if (buf[5][strlen(buf[5]) - 1] == '\r')
                buf[5][strlen(buf[5]) - 1] = '\0';
        }
        // Cache-Control header
        else if (!strncmp(k, req_header[3], strlen(req_header[3]))) {
            buf[6] = k + strlen(req_header[3]);
            if (buf[6][strlen(buf[6]) - 1] == '\r')
                buf[6][strlen(buf[6]) - 1] = '\0';
        }
    }
}

// send http response
int http_response(int conn_sd, char **line_req) {
    char *http_resp = malloc(STR_DIM * STR_DIM * 2 * sizeof(char));
    if (!http_resp)
        error_found("Error in malloc!\n");
    // %d status code; %s status code explain; %s date; %s server; %s content type;
    // %d content's length; %s connection type
    char *header = "HTTP/1.1 %d %s\r\nDate: %s\r\nServer: %s\r\nAccept-Ranges: bytes\r\nContent-Type: %s\r\n"
                   "Content-Length: %d\r\nConnection: %s\r\n\r\n";
    char *t = get_time();
    char *server = "ServerIIWProject";
    char *h;

    memset(http_resp, (int) '\0',STR_DIM * STR_DIM * 2);

    printf("Sono qui 1\n");

    if (!line_req[0] || !line_req[1] || !line_req[2] ||
        ((strncmp(line_req[0], "GET", 3) && strncmp(line_req[0], "HEAD", 4)) ||
         (strncmp(line_req[2], "HTTP/1.1", 8) && strncmp(line_req[2], "HTTP/1.0", 8)))) {
        sprintf(http_resp, header, 400, "Bad Request", t, server, "text/html", strlen(HTML[2]), "close");
        h = http_resp;
        h += strlen(http_resp);
        memcpy(h, HTML[2], strlen(HTML[2]));
        if (send_http_response(conn_sd, http_resp, strlen(http_resp)) == -1) {
            fprintf(stderr, "Error in send_http_response()!\n");
            free_http_mem(t, http_resp);
            return -1;
        }
        return 0;
    }

    printf("Sono qui 2\n");

    if (strncmp(line_req[1], "/", strlen(line_req[1])) == 0) {
        sprintf(http_resp, header, 200, "OK", t, server, "text/html", strlen(HTML[0]), "keep-alive");
        if (strncmp(line_req[0], "HEAD", 4)) {
            h = http_resp;
            h += strlen(http_resp);
            memcpy(h, HTML[0], strlen(HTML[0]));
        }
        if (send_http_response(conn_sd, http_resp, strlen(http_resp)) == -1) {
            fprintf(stderr, "Error in send_http_response()!\n");
            free_http_mem(t, http_resp);
            return -1;
        }
    }
    else {
        struct image_t *i = IMAGES;
        char *p_name;
        if (!(p_name = strrchr(line_req[1], '/')))
            i = NULL;
        ++p_name;
        char *p = TMP_RESIZED_PATH + strlen("/tmp");

        // Finding image in the image_t structure
        while (i) {
            if (!strncmp(p_name, i->name, strlen(i->name))) {
                ssize_t dim = 0;
                char *img_to_send = NULL;

                if (!strncmp(p, line_req[1], strlen(p) - strlen(".XXXXXX"))) {
                    // Looking for resized image
                    if (strncmp(line_req[0], "HEAD", 4)) {
                        img_to_send = get_img(p_name, i->size_r, TMP_RESIZED_PATH);
                        if (!img_to_send) {
                            fprintf(stderr, "Error in get_img\n");
                            free_http_mem(t, http_resp);
                            return -1;
                        }
                    }
                    dim = i->size_r;
                } else {
                    // Looking for image in memory cache
                    char name_cached_img[STR_DIM];
                    memset(name_cached_img, (int) '\0', sizeof(char) * STR_DIM);
                    struct cache_t *c;
                    int accept = q_factor(line_req[5]);
                    if (accept == -1)
                        fprintf(stderr, "Eerror in strtod!\n");
                    int q = accept < 0 ? Q_FACTOR : accept;

                    lock(cache_syn -> mtx);
                    {
                        c = i -> img_c;
                        while (c) {
                            if (c -> q == q) {
                                strcpy(name_cached_img, c -> img_q);
                                // If an image has been accessed, move it on top of the list
                                //  in order to keep the image with less hit in the bottom of the list
                                if (cache_space >= 0 && strncmp(cache_syn -> cache_hit_head -> cache_name,
                                                            name_cached_img, strlen(name_cached_img))) {
                                    struct cache_hit *prev_node, *node;
                                    prev_node = NULL;
                                    node = cache_syn -> cache_hit_tail;
                                    while (node) {
                                        if (!strncmp(node -> cache_name, name_cached_img, strlen(name_cached_img))) {
                                            if (prev_node) {
                                                prev_node -> next_hit = node -> next_hit;
                                            } else {
                                                cache_syn -> cache_hit_tail = cache_syn -> cache_hit_tail -> next_hit;
                                            }
                                            node -> next_hit = cache_syn -> cache_hit_head -> next_hit;
                                            cache_syn -> cache_hit_head -> next_hit = node;
                                            cache_syn -> cache_hit_head = cache_syn -> cache_hit_head -> next_hit;
                                            break;
                                        }
                                        prev_node = node;
                                        node = node -> next_hit;
                                    }
                                }
                                break;
                            }
                            c = c -> next_img_c;
                        }
                        
                        if (!c) {
                            // %s = image's name; %d = factor quality (between 1 and 99)
                            sprintf(name_cached_img, "%s_%d", p_name, q);
                            char path[STR_DIM];
                            memset(path, (int) '\0', STR_DIM);
                            sprintf(path, "%s/%s", TMP_CACHE_PATH, name_cached_img);
                            
                            if (cache_space > 0) {
                                // Cache of limited size, if it has not yet reached the maximum cache size
                                // %s/%s = path/name_image; %d = factor quality; %s/%s = path/name_image;
                                char *format = "convert %s/%s -quality %d %s/%s;exit";
                                char command[STR_DIM];
                                memset(command, (int) '\0', STR_DIM);
                                sprintf(command, format, IMG_PATH, p_name, q, TMP_CACHE_PATH, name_cached_img);
                                if (system(command)) {
                                    fprintf(stderr, "Error in system(): cannot refactoring image %s\n", p_name);
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                struct stat buf;
                                memset(&buf, (int) '\0', sizeof(struct stat));
                                errno = 0;
                                if (stat(path, &buf) != 0) {
                                    if (errno == ENAMETOOLONG) {
                                        fprintf(stderr, "Error in stat(): path too long!\n");
                                        free_http_mem(t, http_resp);
                                        unlock(cache_syn -> mtx);
                                        return -1;
                                    }
                                    fprintf(stderr, "Error in http_response: Invalid path!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                } else if (!S_ISREG(buf.st_mode)) {
                                    fprintf(stderr, "Error in http_response: Non-regular files can not be analysed!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                struct cache_t *new_entry = malloc(sizeof(struct cache_t));
                                struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                                if (!new_entry || !new_hit) {
                                    fprintf(stderr, "Error in malloc!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                memset(new_entry, (int) '\0', sizeof(struct cache_t));
                                memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                                
                                new_entry -> q = q;
                                strcpy(new_entry -> img_q, name_cached_img);
                                new_entry -> size_q = (size_t) buf.st_size;
                                new_entry -> next_img_c = i -> img_c;
                                i -> img_c = new_entry;
                                c = i -> img_c;

                                strncpy(new_hit -> cache_name, name_cached_img, strlen(name_cached_img));
                                if (!cache_syn -> cache_hit_head && !cache_syn -> cache_hit_tail) {
                                    new_hit -> next_hit = cache_syn -> cache_hit_head;
                                    cache_syn -> cache_hit_tail = cache_syn -> cache_hit_head = new_hit;
                                } else {
                                    new_hit -> next_hit = cache_syn -> cache_hit_head -> next_hit;
                                    cache_syn -> cache_hit_head -> next_hit = new_hit;
                                    cache_syn -> cache_hit_head = cache_syn -> cache_hit_head -> next_hit;
                                }
                                cache_space--;
                            } else if (!cache_space) {
                                // Cache full. Deleting the oldest requested element.
                                char name_to_remove[STR_DIM];
                                memset(name_to_remove, (int) '\0', STR_DIM);
                                sprintf(name_to_remove, "%s/%s", TMP_CACHE_PATH,
                                        cache_syn -> cache_hit_tail -> cache_name);

                                // delete old image from file system
                                DIR *dir;
                                struct dirent *ent;
                                errno = 0;
                                dir = opendir(TMP_CACHE_PATH);
                                if (!dir) {
                                    if (errno == EACCES) {
                                        fprintf(stderr, "Error in opendir: Permission denied!\n");
                                        free_http_mem(t, http_resp);
                                        unlock(cache_syn -> mtx);
                                        return -1;
                                    }
                                    fprintf(stderr, "Error in opendir!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                while ((ent = readdir(dir)) != NULL) {
                                    if (ent -> d_type == DT_REG) {
                                        if (!strncmp(ent -> d_name, cache_syn -> cache_hit_tail -> cache_name,
                                                     strlen(cache_syn -> cache_hit_tail -> cache_name))) {
                                            rm_link(name_to_remove);
                                            break;
                                        }
                                    }
                                }
                                if (!ent) {
                                    fprintf(stderr, "Error in http_response: file '%s' not removed\n", name_to_remove);
                                }
                                if (closedir(dir)) {
                                    fprintf(stderr, "Error in closedir!\n");
                                    free(img_to_send);
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                // resizing new image to insert in cache
                                // %s/%s = path/name_image; %d = factor quality; %s/%s = path/name_image;
                                char *format = "convert %s/%s -quality %d %s/%s;exit";
                                char command[STR_DIM];
                                memset(command, (int) '\0', STR_DIM);
                                sprintf(command, format, IMG_PATH, p_name, q, TMP_CACHE_PATH, name_cached_img);
                                if (system(command)) {
                                    fprintf(stderr, "Error in http_response: cannot resizing image!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                struct stat buf;
                                memset(&buf, (int) '\0', sizeof(struct stat));
                                errno = 0;
                                if (stat(path, &buf) != 0) {
                                    if (errno == ENAMETOOLONG) {
                                        fprintf(stderr, "Error in stat(): path too long!\n");
                                        free_http_mem(t, http_resp);
                                        unlock(cache_syn -> mtx);
                                        return -1;
                                    }
                                    fprintf(stderr, "Error in http_response: Invalid path!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                } else if (!S_ISREG(buf.st_mode)) {
                                    fprintf(stderr, "Error in http_response: non-regular files can not be analysed!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                // insert new image in cache
                                struct cache_t *new_entry = malloc(sizeof(struct cache_t));
                                if (!new_entry) {
                                    fprintf(stderr, "Error in malloc!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                memset(new_entry, (int) '\0', sizeof(struct cache_t));
                                new_entry -> q = q;
                                strcpy(new_entry -> img_q, name_cached_img);
                                new_entry -> size_q = (size_t) buf.st_size;
                                new_entry -> next_img_c = i -> img_c;
                                i -> img_c = new_entry;
                                c = i -> img_c;

                                // delete oldest request element from cache structure
                                struct image_t *img_ptr = IMAGES;
                                struct cache_t *cache_ptr, *cache_prev = NULL;
                                char *ext = strrchr(cache_syn -> cache_hit_tail -> cache_name, '_');
                                size_t dim_fin = strlen(ext);
                                char name_remove[STR_DIM];
                                memset(name_remove, (int) '\0', STR_DIM);
                                strncpy(name_remove, cache_syn -> cache_hit_tail -> cache_name,
                                        strlen(cache_syn -> cache_hit_tail -> cache_name) - dim_fin);
                                while (img_ptr) {
                                    if (!strncmp(img_ptr -> name, name_remove, strlen(name_remove))) {
                                        cache_ptr = img_ptr -> img_c;
                                        while (cache_ptr) {
                                            if (!strncmp(cache_ptr -> img_q, cache_syn -> cache_hit_tail -> cache_name,
                                                         strlen(cache_syn -> cache_hit_tail -> cache_name))) {
                                                if (!cache_prev)
                                                    img_ptr -> img_c = cache_ptr -> next_img_c;
                                                else
                                                    cache_prev -> next_img_c = cache_ptr -> next_img_c;
                                                free(cache_ptr);
                                                break;
                                            }
                                            cache_prev = cache_ptr;
                                            cache_ptr = cache_ptr -> next_img_c;
                                        }
                                        if (!cache_ptr) {
                                            fprintf(stderr, "Error! struct cache_t compromised\n"
                                                            "Cache size automatically set to Unlimited\n\t\tfinding: %s\n",
                                                    name_remove);
                                            free_http_mem(t, http_resp);
                                            cache_space = -1;
                                            unlock(cache_syn -> mtx);
                                            return -1;
                                        }
                                        break;
                                    }
                                    img_ptr = img_ptr -> next_img;
                                }
                                if (!img_ptr) {
                                    cache_space = -1;
                                    fprintf(stderr,
                                            "Error in http_response:\n"
                                            "Cache size automatically set to Unlimited\n\t\tfinding: %s\n", name_remove);
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                struct cache_hit *new_hit = malloc(sizeof(struct cache_hit));
                                memset(new_hit, (int) '\0', sizeof(struct cache_hit));
                                if (!new_hit) {
                                    fprintf(stderr, "Error in malloc!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                strncpy(new_hit -> cache_name, name_cached_img, strlen(name_cached_img));
                                struct cache_hit *to_be_removed = cache_syn -> cache_hit_tail;
                                new_hit -> next_hit = cache_syn -> cache_hit_head -> next_hit;
                                cache_syn -> cache_hit_head -> next_hit = new_hit;
                                cache_syn -> cache_hit_head = cache_syn -> cache_hit_head -> next_hit;
                                cache_syn -> cache_hit_tail = cache_syn -> cache_hit_tail -> next_hit;
                                free(to_be_removed);
                            } else {
                                // Case of unlimited cache
                                // %s/%s = path/name_image; %d = factor quality: %s/%s = path/name_image;
                                char *format = "convert %s/%s -quality %d %s/%s;exit";
                                char command[STR_DIM];
                                memset(command, (int) '\0', STR_DIM);
                                sprintf(command, format, IMG_PATH, p_name, q, TMP_CACHE_PATH, name_cached_img);
                                if (system(command)) {
                                    fprintf(stderr, "Error in http_response: cannot resizing image!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                struct stat buf;
                                memset(&buf, (int) '\0', sizeof(struct stat));
                                errno = 0;
                                if (stat(path, &buf) != 0) {
                                    if (errno == ENAMETOOLONG) {
                                        fprintf(stderr, "Error in stat(): path too long!\n");
                                        free_http_mem(t, http_resp);
                                        unlock(cache_syn -> mtx);
                                        return -1;
                                    }
                                    fprintf(stderr, "Error in stat(): invalid path!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                } else if (!S_ISREG(buf.st_mode)) {
                                    fprintf(stderr, "Error in http_response: non-regular files can not be analysed!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }

                                struct cache_t *new_entry = malloc(sizeof(struct cache_t));
                                memset(new_entry, (int) '\0', sizeof(struct cache_t));
                                if (!new_entry) {
                                    fprintf(stderr, "Error in malloc!\n");
                                    free_http_mem(t, http_resp);
                                    unlock(cache_syn -> mtx);
                                    return -1;
                                }
                                new_entry -> q = q;
                                strcpy(new_entry -> img_q, name_cached_img);
                                new_entry -> size_q = (size_t) buf.st_size;
                                new_entry -> next_img_c = i -> img_c;
                                i -> img_c = new_entry;
                                c = i -> img_c;
                            }
                        }
                    } unlock(cache_syn -> mtx);

                    if (strncmp(line_req[0], "HEAD", 4)) {
                        DIR *dir;
                        struct dirent *ent;
                        errno = 0;
                        dir = opendir(TMP_CACHE_PATH);
                        if (!dir) {
                            if (errno == EACCES) {
                                fprintf(stderr, "Error in opendir: permission denied!\n");
                                free_http_mem(t, http_resp);
                                return -1;
                            }
                            fprintf(stderr, "Error in opendir()!\n");
                            free_http_mem(t, http_resp);
                            return -1;
                        }
                        while ((ent = readdir(dir)) != NULL) {
                            if (ent -> d_type == DT_REG) {
                                if (!strncmp(ent -> d_name, name_cached_img, strlen(name_cached_img))) {
                                    img_to_send = get_img(name_cached_img, c -> size_q, TMP_CACHE_PATH);
                                    if (!img_to_send) {
                                        fprintf(stderr, "Error in get_img()!\n");
                                        free_http_mem(t, http_resp);
                                        return -1;
                                    }
                                    break;
                                }
                            }
                        }
                        if (closedir(dir)) {
                            fprintf(stderr, "data_to_send: Error in closedir\n");
                            free(img_to_send);
                            free_http_mem(t, http_resp);
                            return -1;
                        }
                    }
                    dim = c -> size_q;
                }

                sprintf(http_resp, header, 200, "OK", t, server, "image/gif", dim, "keep-alive");
                ssize_t dim_tot = (size_t) strlen(http_resp);
                if (strncmp(line_req[0], "HEAD", 4)) {
                    if (dim_tot + dim > STR_DIM * STR_DIM * 2) {
                        http_resp = realloc(http_resp, (dim_tot + dim) * sizeof(char));
                        if (!http_resp) {
                            fprintf(stderr, "Error in realloc!\n");
                            free_http_mem(t, http_resp);
                            free(img_to_send);
                            return -1;
                        }
                        memset(http_resp + dim_tot, (int) '\0', (size_t) dim);
                    }
                    h = http_resp;
                    h += dim_tot;
                    memcpy(h, img_to_send, (size_t) dim);
                    dim_tot += dim;
                }
                if (send_http_response(conn_sd, http_resp, dim_tot) == -1) {
                    fprintf(stderr, "Error in http_response: cannot sending data to client!\n");
                    free_http_mem(t, http_resp);
                    return -1;
                }
                free(img_to_send);
                break;
            }
            i = i -> next_img;
        }

        if (!i) {
            sprintf(http_resp, header, 404, "Not Found", t, server, "text/html", strlen(HTML[1]), "close");
            if (strncmp(line_req[0], "HEAD", 4)) {
                h = http_resp;
                h += strlen(http_resp);
                memcpy(h, HTML[1], strlen(HTML[1]));
            }
            if (send_http_response(conn_sd, http_resp, strlen(http_resp)) == -1) {
                fprintf(stderr, "Error in send_http_response!\n");
                free_http_mem(t, http_resp);
                return -1;
            }
        }
    }
    free_http_mem(t, http_resp);
    return 0;
}

// Used to send HTTP response to clients. Return the size of response.
ssize_t send_http_response(int conn_sd, char *s, ssize_t dim) {
    ssize_t sent = 0;
    char *msg = s;
    while (sent < dim) {
        sent = send(conn_sd, msg, (size_t) dim, MSG_NOSIGNAL);
        if (sent <= 0)
            break;
        msg += sent;
        dim -= sent;
    }
    return sent;
}

// free memory use for http handler
void free_http_mem(char *time, char *http) {
    free(time);
    free(http);
}

// Find q factor from Accept header
// Return values: -1 --> error
//                -2 --> factor quality not specified in the header
int q_factor(char *h_accept) {
    double images, others, q;
    images = others = q = -2.0;
    char *chr, *t1 = strtok(h_accept, ",");
    if (!h_accept || !t1)
        return (int) (q *= 100);

    do {
        while (*t1 == ' ')
            t1++;
        if (!strncmp(t1, "image", strlen("image"))) {
            chr = strrchr(t1, '=');
            // If not specified the 'q' value or if there was an error in transmission, the default value of 'q' is 1.0
            if (!chr) {
                images = 1.0;
                break;
            } else {
                errno = 0;
                double tmp = strtod(++chr, NULL);
                if (tmp > images)
                    images = tmp;
                if (errno != 0)
                    return -1;
            }
        } else if (!strncmp(t1, "*", strlen("*"))) {
            chr = strrchr(t1, '=');
            if (!chr) {
                others = 1.0;
            } else {
                errno = 0;
                others = strtod(++chr, NULL);
                if (errno != 0)
                    return -1;
            }
        }
    } while ((t1 = strtok(NULL, ",")));

    if (images > others || (others > images && images != -2.0))
        q = images;
    else if (others > images && images == -2.0)
        q = others;
    else
        fprintf(stderr, "string: %s\t\tquality: Unexpected error\n", h_accept);

    return (int) (q *= 100);
}