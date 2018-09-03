//
// Created by emanuele on 31/08/18.
//
#include "server_init.h"

void server_init(int argc, char **argv);

int main(int argc, char **argv) {
    server_init(argc, argv);
}

// Do all the operation to initialize the server
void server_init(int argc, char **argv) {
    // setting paths of images e log file
    // manage option passed by command line
    manage_option(argc, argv);
    // initialize thread support
}