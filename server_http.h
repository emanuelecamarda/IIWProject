//
// Created by Emanuele on 08/09/2018.
//

#ifndef IIWPROJECT_SERVER_HTTP_H
#define IIWPROJECT_SERVER_HTTP_H

#include <sys/socket.h>

void analyze_http_request(int conn_sd, struct sockaddr_in cl_addr);
void split_http_request(char *s, char **buf);
int http_response(int conn_sd, char **line);
ssize_t send_http_response(int conn_sd, char *s, ssize_t dim);
void free_http_mem(char *time, char *http);
int q_factor(char *h_accept);

#endif //IIWPROJECT_SERVER_HTTP_H