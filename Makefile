CC=gcc
CFLAGS=-Wall -pthread -D_REENTRANT -g -O0
DEPS = header.h
OBJ = projeto.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

projeto: $(OBJ)
		$(CC) -o $@ $^ $(CFLAGS)