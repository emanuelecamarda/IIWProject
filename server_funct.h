//
// Created by emanuele on 31/08/18.
//

#ifndef PROGETTO_SERVER_FUNCT_H
#define PROGETTO_SERVER_FUNCT_H

#include "utils.h"
#include "threads_work.h"

void manage_option(int argc, char **argv);
void manage_signal(void);
void set_default_path(void);
void struct_init(void);
void server_start(void);
void image_resize(void);
void html_create(void);
void main_th_work(void);

#endif //PROGETTO_SERVER_FUNCT_H
