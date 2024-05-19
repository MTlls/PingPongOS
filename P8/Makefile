CFLAGS = -Wall

# vai de voce querer montar um programa que vai usar o test ou o test1
OBJS = ppos_core.o test.o queue.o
PROG = ppos

# Lista de arquivos para distribuição
DISTFILES = ppos_core.c ppos_data.h queue.c
DISTDIR = `basename ${PWD}`

all: $(PROG)

$(PROG): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

ppos_core.o: ppos_core.c ppos.h ppos_data.h queue.h

p4.o: ppos.h 

queue.o: queue.c queue.h

debug: CFLAGS += -g -DDEBUG
debug: all

clean:
	@echo "Limpando ...."
	@rm -f *~ *.bak *.tmp core

purge: clean
	@echo "Faxina ...."
	@rm -f  $(PROG) *.o *.out a.out $(DISTDIR) $(DISTDIR).tar

dist: purge
	@echo "Gerando arquivo de distribuição ($(DISTDIR).tar) ..."
	@ln -s . $(DISTDIR)
	@tar -czvf $(DISTDIR).tar.gz $(DISTFILES)
	@rm -f $(DISTDIR)