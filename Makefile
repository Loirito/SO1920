CC=gcc
CFLAGS=-Wall -pthread -D_REENTRANT
DEPS = header.h
OBJ = projeto.o

%.o: %.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS)

projeto: $(OBJ)
		$(CC) -o $@ $^ $(CFLAGS)