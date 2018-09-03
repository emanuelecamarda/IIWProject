//
// Created by emanuele on 31/08/18.
//

#ifndef PROGETTO_UTILS_H
#define PROGETTO_UTILS_H

int PORT;
char *USAGE_MSG;

void error_found(char *msg);
void writef(char *s, FILE *file);
void free_mem();

#endif //PROGETTO_UTILS_H
