CFLAGS = -Wextra

OBJS = ppos_core.o pingpong-dispatcher.o queue.o
PROG = ppos

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

ppos_core.o: ppos_core.c ppos.h ppos_data.h queue.h

pingpong-dispatcher.o: ppos.h 

queue.o: queue.c queue.h

debug: CFLAGS += -g -DDEBUG
debug: all

