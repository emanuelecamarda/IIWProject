CC = gcc
CFLAGS = -pthread -O3 -Wall -Wextra
DEPS = server_funct.h utils.h threads_work.h
OBJ = server_main.o server_funct.o utils.o threads_work.o

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server : $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY : clean

clean :
	-rm -f $(OBJ) core *~
