//
// Created by emanuele on 06/09/18.
//

#ifndef IIWPROJECT_THREADS_WORK_H
#define IIWPROJECT_THREADS_WORK_H

void create_th(void * (*work) (void *), void *k);
void th_init(void *arg);
void lock(pthread_mutex_t *mtx);
void unlock(pthread_mutex_t *mtx);
void wait_cond(pthread_cond_t *cond, pthread_mutex_t *mtx);
void signal_cond(pthread_cond_t *cond);
void th_scaling_up(void);
void th_scaling_down(void);
void *manage_input(void *arg);
void *manage_connection(void *arg);

#endif //IIWPROJECT_THREADS_WORK_H
