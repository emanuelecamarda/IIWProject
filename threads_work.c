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
#include "threads_work.h"

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

// initialize arg thread with the work manage_connection. Needed th_syn locked
void th_init(void *arg) {
    int i, j, *n = (int *) arg;

    for (j = 0, i = 0; j < *n && i < MAX_CONN_NUM; i++) {
        if (th_syn->clients[i] == -2) {
            if (PRINT_DUMP)
                printf("Creating thread %d\n", i);
            create_th(manage_connection, &i);
            while (th_syn->clients[i] != -1) {
                if (PRINT_DUMP)
                    printf("Wait on th_syn cond[%d] in th_init\n", i);
                wait_cond(th_syn->cond + i, th_syn->mtx);
            }
            j++;
        }
    }
}

// kill arg threads
void th_kill(void *arg) {
    int i, j, *n = (int *) arg;

    lock(th_syn -> mtx);
    if (PRINT_DUMP)
        printf("Lock th_syn in th_kill\n");
    {
        for (i = 0, j = 0; j < *n && i < MAX_CONN_NUM; i++) {
            if (th_syn->clients[i] == -1) {
                if (PRINT_DUMP)
                    printf("Killing thread %d\n", i);
                th_syn -> clients[i] = -2;
                if (PRINT_DUMP)
                    printf("signal th_syn cond[%d] in th_kill\n", i);
                signal_cond(th_syn -> cond + i);
                while (th_syn -> clients[i] != -1) {
                    if (PRINT_DUMP)
                        printf("Wait on th_syn cond[%d] in th_kill\n", i);
                    wait_cond(th_syn->cond + i, th_syn->mtx);
                }
                j++;
            }
        }
    } unlock(th_syn->mtx);
    if (PRINT_DUMP)
        printf("Unlock th_syn in th_kill\n");
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
    if (pthread_cond_wait(cond, mtx) != 0)
        error_found("Error in pthread_cond_wait!\n");
}

// Used to send a signal for a condition
void signal_cond(pthread_cond_t *cond) {
    if (pthread_cond_signal(cond) != 0)
        error_found("Error in pthread_cond_signal!\n");
}

// use to create new thread if necessary. Needed th_syn locked
void th_scaling_up(void) {
    lock(state_syn -> mtx);
    {
        if (PRINT_DUMP)
            printf("Lock state_syn in scaling up\n");

        if (((float) state_syn -> conn_num) / ((float) state_syn -> init_th_num) * 100 > TH_SCALING_UP) {
            int n, i, j;
            if ((state_syn -> init_th_num) + MIN_TH_NUM <= MAX_CONN_NUM) {
                n = MIN_TH_NUM;
            } else {
                n = MAX_CONN_NUM - state_syn -> init_th_num;
            }
            if (n) {
                state_syn -> init_th_num += n;
                unlock(state_syn -> mtx);
                for (j = 0, i = 0; j < n && i < MAX_CONN_NUM; i++) {
                    if (th_syn->clients[i] == -2) {
                        if (PRINT_DUMP)
                            printf("Creating thread %d\n", i);
                        create_th(manage_connection, &i);
                        while (th_syn->clients[i] != -1) {
                            if (PRINT_DUMP)
                                printf("Wait on th_syn cond[%d] in th_init\n", i);
                            wait_cond(th_syn->cond + i, th_syn->mtx);
                        }
                        j++;
                    }
                }
                return;
            }
        }
    }
    unlock(state_syn -> mtx);
    if (PRINT_DUMP)
        printf("Unlock state_syn in scaling up\n");
}

// Used to kill threads if necessary
void th_scaling_down(void) {
    int n = 0, i;

    lock(state_syn->mtx);
    if (PRINT_DUMP)
        printf("Lock state_syn in scaling down\n");
    if (((float) state_syn->conn_num) / ((float) state_syn->init_th_num) * 100 < TH_SCALING_DOWN) {
        if (state_syn->init_th_num - MIN_TH_NUM >= MIN_TH_NUM) {
            n = MIN_TH_NUM;
        } else {
            n = state_syn->init_th_num - MIN_TH_NUM;
        }
    }
    if (n) {
        state_syn -> init_th_num -= n;
        unlock(state_syn->mtx);
        if (PRINT_DUMP)
            printf("Unlock state_syn in scaling down\n");
        for (i = 0; i < n; i++) {
            th_kill(&n);
        }
        return;
    }
    unlock(state_syn->mtx);
    if (PRINT_DUMP)
        printf("Unlock state_syn in scaling down\n");
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
                lock(state_syn -> mtx);
                if (PRINT_DUMP)
                    printf("Input thread lock state_syn\n");
                {
                    conn = state_syn -> conn_num;
                    num_th = state_syn -> init_th_num;
                } unlock(state_syn -> mtx);
                if (PRINT_DUMP)
                    printf("Input thread unlock state_syn\n");
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
                    if (PRINT_DUMP)
                        printf("Input thread lock th_syn\n");
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
                    if (PRINT_DUMP)
                        printf("Input thread unlock th_syn\n");
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
    if (pthread_detach(pthread_self()) != 0)
        error_found("Error in pthread_detach!\n");

    lock(state_syn -> mtx);
    if (PRINT_DUMP)
        printf("Thread %d lock state_syn\n", *my_index);
    {
        state_syn -> init_th_num++;
    } unlock(state_syn -> mtx);
    if (PRINT_DUMP)
        printf("Thread %d unlock state_syn\n", *my_index);

    lock(th_syn -> mtx);
    if (PRINT_DUMP)
        printf("Thread %d lock th_syn\n", *my_index);

    // Thread ready for incoming connections
    th_syn -> clients[*my_index] = -1;
    if (PRINT_DUMP)
        printf("Thread %d signal th_syn cond[%d]\n", *my_index, *my_index);
    signal_cond(th_syn -> cond + *my_index);


    // Deal connections
    while (1) {
        memset(&cl_addr, (int) '\0', sizeof(struct sockaddr_in));
        if (PRINT_DUMP)
            printf("Thread %d wait on th_syn cond[%d]\n", *my_index, *my_index);
        wait_cond(th_syn -> cond + *my_index, th_syn -> mtx);
        conn_sd = th_syn -> clients[*my_index];
        if (conn_sd == -1) {
            error_found("Unknown error in manage threads!");
        }
        if (conn_sd == -2) {
            th_syn -> clients[*my_index] = -1;
            unlock(th_syn -> mtx);
            if (PRINT_DUMP)
                printf("Thread %d unlock th_syn\n", *my_index);
            break;
        }
        memcpy(&cl_addr, &th_syn -> cl_addr, sizeof(struct sockaddr_in));
        th_syn -> accept = 1;

        lock(state_syn -> mtx);
        {
            if (PRINT_DUMP)
                printf("Thread %d lock state_syn\n", *my_index);
            state_syn->conn_num++;
        }
        unlock(state_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d unlock state_syn\n", *my_index);

        th_scaling_up();

        if (PRINT_DUMP)
            printf("Thread %d signal th_syn cond[%d]\n", *my_index, *my_index);
        signal_cond(th_syn -> cond + *my_index);
        unlock(th_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d unlock th_syn\n", *my_index);

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

        lock(state_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d lock state_syn\n", *my_index);
        {
            state_syn -> conn_num--;
            if (PRINT_DUMP)
                printf("Thread %d signal state_syn\n", *my_index);
            signal_cond(state_syn -> cond);
        } unlock(state_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d unlock state_syn\n", *my_index);

        lock(th_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d lock th_syn\n", *my_index);
        {
            th_syn -> clients[*my_index] = -1;
        } unlock(th_syn -> mtx);
        if (PRINT_DUMP)
            printf("Thread %d unlock th_syn\n", *my_index);

        th_scaling_down();
    }
    free(my_index);
    pthread_exit(EXIT_SUCCESS);
}
