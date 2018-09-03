//
// Created by emanuele on 31/08/18.
//

int PORT = 11111;
char *USAGE_MSG =
        "Usage: %s [option]\n\n"
        "\tThis is the list of the possible option:"
        "\t\t-p port number, default value: 11111\n"
        "\t\t-i image directory's path, default: current directory\n"
        "\t\t-l log file directory's path, default: current directory\n"
        "\t\t-n initial thread number, default value: \n"
        "\t\t-m maximum thread number, default value: \n"
        "\t\t-r resize images' percentage, default value: 20\n"
        "\t\t-c maximum number of image in cache, default value: \n"
        "\t\t-h help\n";

// write on stderr and on log file the error that occurs and exit with failure
void error_found(char *msg) {
    fprintf(stderr, "%s", s);
    if (LOG) {
        write_on_stream(s, LOG);
    }
    free_mem();
    exit(EXIT_FAILURE);
}

// write the string s on a file
void writef(char *s, FILE *file) {
    size_t n;
    size_t len = strlen(s);

    while ((n = fwrite(s, sizeof(char), len, file)) < len) {
        if (ferror(file)) {
            fprintf(stderr, "Error in fwrite\n");
            exit(EXIT_FAILURE);
        }
        len -= n;
        s += n;
    }
}

//TODO
// dealloc all memory used by server
void free_mem() {

}