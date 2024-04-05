CFLAGS = -Wextra

OBJS = ppos_core.o p4.o queue.o
PROG = ppos

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

ppos_core.o: ppos_core.c ppos.h ppos_data.h queue.h

p4.o: ppos.h 

queue.o: queue.c queue.h

debug: CFLAGS += -g -DDEBUG
debug: all

