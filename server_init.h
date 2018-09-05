//
// Created by emanuele on 31/08/18.
//

#ifndef PROGETTO_SERVER_INIT_H
#define PROGETTO_SERVER_INIT_H

#include "utils.h"

extern char *USAGE_MSG;

void manage_option(int argc, char **argv);
void manage_signal(void);
void set_default_path(void);
void struct_init(void);
void server_start(void);
void image_resize(void);
void html_create(void);

#endif //PROGETTO_SERVER_INIT_H
