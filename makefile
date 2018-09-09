CC = gcc
CFLAGS = -pthread -O3 -Wall -Wextra
DEPS = server_funct.h utils.h threads_work.h server_http.h
OBJ = server_main.o server_funct.o utils.o threads_work.o server_http.o

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server : $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY : clean

clean :
	-rm -f $(OBJ) core *~
