gCC = gcc -Wall

OBJS = queue.o testafila.o
PROG = teste

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)
queue.o: queue.c queue.h

testafila.o: queue.h testafila.c

