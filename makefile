CC = gcc
CFLAGS = -pthread -O3 -Wall -Wextra
DEPS = server_init.h utils.h
OBJ = server.o server_init.o utils.o

%.o : %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

server : $(OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

.PHONY : clean

clean :
	-rm -f $(OBJ) core *~
