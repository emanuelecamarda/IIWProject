//
// Created by emanuele on 06/09/18.
//

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <unistd.h>

#include "utils.h"
#include "server_http.h"

void lock(pthread_mutex_t *mtx);
void unlock(pthread_mutex_t *mtx);
void wait_cond(pthread_cond_t *cond, pthread_mutex_t *mtx);
void signal_cond(pthread_cond_t *cond);
void *manage_input(void *arg);
void *manage_connection(void *arg);

void create_th(void * (*work) (void *), void *k) {
    pthread_t tid;
    errno = 0;
    if (pthread_create(&tid, NULL, work, k) != 0) {
        if (errno == EAGAIN || errno == ENOMEM)
            error_found("Error: insufficient resources to create another thread!\n");
        else
            error_found("Error in pthread_create!\n");
    }
}

// initialize arg thread with the work manage_connection
void th_init(void *arg) {
    int i, *n = (int *) arg;

    lock(th_syn -> mtx);
    {
        for (i = 0; i < *n; i++) {
            if (th_syn->clients[i] == -2) {
                create_th(manage_connection, &i);
                while (th_syn->clients[i] != -1) {
                    wait_cond(th_syn->cond, th_syn->mtx);
                }
            }
        }
    } unlock(th_syn->mtx);
}

// kill arg threads
void th_kill(void *arg) {
    int i, j, *n = (int *) arg;

    lock(th_syn -> mtx);
    {
        for (i = 0, j = 0; j < *n && i < MAX_CONN_NUM; i++) {
            if (th_syn->clients[i] == -1) {
                th_syn -> clients[i] = -2;
                lock(accept_conn_syn -> mtx); {
                    while (accept_conn_syn -> conn_sd != -1)
                        wait_cond(accept_conn_syn -> cond, accept_conn_syn -> mtx);
                    accept_conn_syn -> conn_sd = -2;
                    signal_cond(accept_conn_syn -> cond + i);
                    while (accept_conn_syn -> conn_sd != -1)
                        wait_cond(accept_conn_syn -> cond, accept_conn_syn -> mtx);
                } unlock(accept_conn_syn -> mtx);
                j++;
            }
        }
    } unlock(th_syn->mtx);
}

//  Use by a thread to lock a mutex
void lock(pthread_mutex_t *mtx) {
    if (pthread_mutex_lock(mtx) != 0)
        error_found("Error in pthread_mutex_lock!\n");
}

// Use by a thread to unlock a mutex
void unlock(pthread_mutex_t *mtx) {
    if (pthread_mutex_unlock(mtx) != 0)
        error_found("Error in pthread_mutex_unlock!\n");
}

// Used to block a thread on a condition locked by a mutex
void wait_cond(pthread_cond_t *cond, pthread_mutex_t *mtx) {
    printf("Sono in wait_cond 1\n");
    if (pthread_cond_wait(cond, mtx) != 0)
        error_found("Error in pthread_cond_wait!\n");

    printf("Sono in wait_cond 2\n");

}

// Used to send a signal for a condition
void signal_cond(pthread_cond_t *cond) {
    if (pthread_cond_signal(cond) != 0)
        error_found("Error in pthread_cond_signal!\n");
}

// use to create new thread if necessary
void th_scaling_up(void) {
    lock(state_syn -> mtx);
    {
        if (state_syn -> conn_num / state_syn -> init_th_num * 100 >  TH_SCALING_UP) {
            int n, i;
            if (state_syn -> init_th_num + MIN_TH_NUM <= MAX_CONN_NUM) {
                n = MIN_TH_NUM;
            } else {
                n = MAX_CONN_NUM - state_syn -> init_th_num;
            }
            if (n) {
                for (i = 0; i < n; i++) {
                    th_init(&n);
                }
                state_syn -> init_th_num += n;
            }
        }
    } unlock(state_syn -> mtx);
}

// Used to kill threads if necessary
void th_scaling_down(void) {
    int n = 0, i;

    lock(state_syn->mtx);
    {
        if (state_syn->conn_num / state_syn->init_th_num * 100 < TH_SCALING_DOWN) {
            if (state_syn->init_th_num - MIN_TH_NUM >= MIN_TH_NUM) {
                n = MIN_TH_NUM;
            } else {
                n = state_syn->init_th_num - MIN_TH_NUM;
            }
        }
    } unlock(state_syn->mtx);
    if (n) {
        for (i = 0; i < n; i++) {
            th_kill(&n);
        }
    }
}

// Thread works to manage command line input
void *manage_input(void *arg) {
    if (arg != NULL) {
        error_found("Unknown error!");
    }
    char cmd[2];
    int conn, num_th, i;

    printf("\n%s\n", USER_OPT);
    while (1) {
        memset(cmd, (int) '\0', 2);
        if (fscanf(stdin, "%s", cmd) != 1)
            error_found("Error in fscanf!\n");
        if (strlen(cmd) != 1) {
            printf("Input not valid!\n%s\n", USER_OPT);
        } else {
            if (cmd[0] == 's' || cmd[0] == 'S') {
                lock(state_syn -> mtx); {
                    conn = state_syn -> conn_num;
                    num_th = state_syn -> init_th_num;
                } unlock(state_syn -> mtx);
                fprintf(stdout, "\nConnections' number: %d\n"
                                "Threads running: %d\n\n", conn, num_th);
                continue;
            } else if (cmd[0] == 'q' || cmd[0] == 'Q') {
                fprintf(stdout, "Closing server...\n");
                errno = 0;
                if (close(LISTEN_SD) != 0) {
                    if (errno == EIO)
                        error_found("I/O error occurred\n");
                    error_found("Error in close\n");
                }
                for (i = 0; i < MAX_CONN_NUM; i++) {
                    lock(th_syn -> mtx);
                    {
                        if (th_syn -> clients[i] >= 0) {
                            if (close(th_syn -> clients[i]) != 0) {
                                switch (errno) {
                                    case EIO:
                                        error_found("Error in close: I/O error\n");
                                        break;
                                    case ENOTCONN:
                                        error_found("Error in close: the socket is not connected\n");
                                        break;
                                    case EBADF:
                                        fprintf(stderr, "Error in close: bad file number. Probably client "
                                                        "has disconnected\n");
                                        break;
                                    default:
                                        error_found("Error in close!\n");
                                        break;
                                }
                            }
                        }
                    } unlock(th_syn -> mtx);
                }
                write_log("\t\tServer closed.\n\n\n");

                errno = 0;
                if (fflush(LOG)) {
                    if (errno == EBADF)
                        fprintf(stderr, "Error in fflush!\n");
                    exit(EXIT_FAILURE);
                }

                if (fclose(LOG) != 0)
                    error_found("Error in fclose\n");

                free_mem();

                exit(EXIT_SUCCESS);
            }
            printf("\n%s\n", USER_OPT);
        }
    }
}

// Threads work to manage connection
void *manage_connection(void *arg) {
    struct sockaddr_in cl_addr;
    int *my_index = malloc(sizeof(int)), conn_sd;
    *my_index = * (int *) arg;

    printf("my index: %d\n", *my_index);

    if (pthread_detach(pthread_self()) != 0)
        error_found("Error in pthread_detach!\n");

    printf("Sono in manage connection 1\n");

    lock(th_syn -> mtx);
    {
        // Thread ready for incoming connections
        th_syn -> clients[*my_index] = -1;

        printf("my index: %d\n", *my_index);

        lock(state_syn -> mtx);
        {
            state_syn -> init_th_num++;
        } unlock(state_syn -> mtx);
        signal_cond(th_syn -> cond);
    } unlock(th_syn -> mtx);

    // Deal connections
    while (1) {

        printf("Sono in manage connection 2\n");

        memset(&cl_addr, (int) '\0', sizeof(struct sockaddr_in));
        lock(accept_conn_syn -> mtx); {
            wait_cond(accept_conn_syn -> cond + *my_index, accept_conn_syn -> mtx);

            printf("my index: %d\n", *my_index);

            printf("Sono in manage connection 3\n");

            conn_sd = accept_conn_syn -> conn_sd;
            if (conn_sd == -2) {
                accept_conn_syn -> conn_sd = -1;
                unlock(accept_conn_syn -> mtx);
                break;
            }

            printf("Sono in manage connection 4\n");

            memcpy(&cl_addr, &accept_conn_syn -> cl_addr, sizeof(struct sockaddr_in));

            printf("Sono in manage connection 5\n");

            lock(state_syn -> mtx); {
                state_syn -> conn_num++;
            } unlock(state_syn -> mtx);

            printf("Sono in manage connection 6\n");

            accept_conn_syn -> conn_sd = -1;

            printf("my index: %d\n", *my_index);

            signal_cond(accept_conn_syn -> cond + *my_index);

            printf("Sono in manage connection 7\n");

        } unlock(accept_conn_syn -> mtx);

        th_scaling_up();

        analyze_http_request(conn_sd, cl_addr);

        errno = 0;
        if (close(conn_sd) != 0) {
            switch (errno) {
                case EIO:
                    fprintf(stderr, "Error in close(): I/O error occurred!\n");
                    break;

                case EBADF:
                    fprintf(stderr, "Error in close(): bad file number (%d). Probably client has disconnected..\n",
                            conn_sd);
                    break;

                default:
                    fprintf(stderr, "Error in close()!\n");
                    break;
            }
        }

        lock(state_syn -> mtx); {
            state_syn -> conn_num--;
            signal_cond(state_syn -> cond);
        } unlock(state_syn -> mtx);

        lock(th_syn -> mtx); {
            th_syn -> clients[*my_index] = -1;
        } unlock(th_syn -> mtx);

        th_scaling_down();
    }
    free(my_index);
    pthread_exit(EXIT_SUCCESS);
}
