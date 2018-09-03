//
// Created by emanuele on 31/08/18.
//
#include <stdio.h>
#include <stdlib.h>

#include "utils.h"

// manage option passed by users with command line
void manage_option(int argc, char **argv) {
    int i;

    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, USAGE_MSG, argv[0]);
            exit(EXIT_SUCCESS);
        }
    }

    while ((c = getopt(argc, argv, "p:l:i:n:m:r:c:")) != -1) {
        switch (c) {
            case 'p':
                errno = 0;
                int p_arg = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0' || p_arg > 65535 || p_arg < 1025) {
                    error_found("Error in option -p: port number must be a integer between 1025 and 65,535\n");
                }
                PORT = p_arg;
                break;
//TODO breakpoint
            case 'l':
                get_info(&statbuf, optarg, 1);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(path[0], optarg, strlen(optarg));
                } else {
                    strncpy(path[0], optarg, strlen(optarg) - 1);
                }

                if (path[0][strlen(path[0])] != '\0')
                    path[0][strlen(path[0])] = '\0';
                break;

            case 'i':
                get_info(&statbuf, optarg, 1);

                if (optarg[strlen(optarg) - 1] != '/') {
                    strncpy(path[1], optarg, strlen(optarg));
                } else {
                    strncpy(path[1], optarg, strlen(optarg) - 1);
                }

                if (path[1][strlen(path[1])] != '\0')
                    path[1][strlen(path[1])] = '\0';
                break;

            case 't':
                errno = 0;
                int t_arg = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -t: Error in strtol: Invalid number\n");
                if (t_arg < 2)
                    error_found("Argument -t: Attention: due to performance problem, thread's numbers must be >= 2!\n");
                MINTH = t_arg;
                mod_t = 1;
                break;

            case 'c':
                errno = 0;
                int c_arg = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -c: Error in strtol: Invalid number\n");
                if (c_arg < 1)
                    error_found("Argument -c: Error: maximum connections' number must be > 0!");
                MAXCONN = c_arg;
                mod_c = 1;
                break;

            case 'r':
                errno = 0;
                *perc = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -r: Error in strtol: Invalid number\n");
                if (*perc < 1 || *perc > 100)
                    error_found("Argument -r: The number must be >=1 and <= 100\n");
                break;

            case 'n':
                errno = 0;
                int cache_size = (int) strtol(optarg, &e, 10);
                if (errno != 0 || *e != '\0')
                    error_found("Argument -n: Error in strtol: Invalid number\n");
                if (cache_size)
                    CACHE_N = cache_size;
                break;

            case '?':
                error_found("Invalid argument\n");

            default:
                error_found("Unknown error in getopt\n");
        }
    }

}
