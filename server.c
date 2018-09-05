//
// Created by emanuele on 31/08/18.
//
#include <stdlib.h>

#include "server_init.h"
#include "utils.h"

void server_init(int argc, char **argv);

int main(int argc, char **argv) {
    server_init(argc, argv);

    exit(EXIT_SUCCESS);
}

// Do all the operation to initialize the server
void server_init(int argc, char **argv) {
    if(PRINT_DUMP)
        printf("Start\n");
    // setting paths of images e log file
    set_default_path();
    if (PRINT_DUMP)
        printf("Finish set_default_path()\n");
    // manage signal
    manage_signal();
    if (PRINT_DUMP)
        printf("Finish manage_signal()\n");
    // manage option passed by command line
    manage_option(argc, argv);
    if (PRINT_DUMP)
        printf("Finish manage_option()\n");
    // initialize thread and struct support
    struct_init();
    if (PRINT_DUMP)
        printf("Finish struct_init()\n");
    // starting server
    server_start();
    if (PRINT_DUMP)
        printf("Finish server_start()\n");
    // resize images
    image_resize();
    if (PRINT_DUMP)
        printf("Finish image_resize()\n");
    // create html file
    html_create();
    if (PRINT_DUMP)
        printf("Finish html_create()\n");
}