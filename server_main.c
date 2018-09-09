//
// Created by emanuele on 31/08/18.
//
#include <stdlib.h>
#include <stdio.h>

#include "server_funct.h"
#include "utils.h"

void init(int argc, char **argv);

int main(int argc, char **argv) {

    init(argc, argv);

    server_work();

    exit(EXIT_SUCCESS);
}

// Do all the operation to initialize the server
void init(int argc, char **argv) {
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
    // create thread to manage user's input
    create_th(manage_input, NULL);
    if (PRINT_DUMP)
        printf("Finish create_th(manage_input, NULL)\n");
    // create starting threads
    th_init(&MIN_TH_NUM);
    if (PRINT_DUMP)
        printf("Finish th_init(MIN_TH_NUM)\n");
    if (PRINT_DUMP)
        printf("\nPort number: %d\nImages' path: %s\nLog file's path: %s\nMinimum threads' number: %d\n"
               "Maximum connection's number: %d\nResize images' percentage: %d\nCache size: %d\nScaling up: %d\n"
               "Scaling down: %d\n", PORT, IMG_PATH, LOG_PATH, MIN_TH_NUM, MAX_CONN_NUM, RESIZE_PERC, cache_space,
               TH_SCALING_UP, TH_SCALING_DOWN);
}