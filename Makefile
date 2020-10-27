#VERSION=2.huge

CC  := gcc
BIN := =/usr/local/bin

ifeq (0, ${MAKELEVEL})
TIMESTAMP=$(shell date)
endif

ifeq (1, ${DEBUG})
CFLAGS=-g3 -W -Wall -Wno-unused-but-set-variable -O0 -DTIMESTAMP="$(TIMESTAMP)" -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -mpopcnt -msse4.2
else
CFLAGS=-g3 -W -Wall -Wno-unused-but-set-variable -O4 -DTIMESTAMP="$(TIMESTAMP)" -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -mpopcnt -msse4.2
endif

GLIBS=-lm -lrt -lpthread
GENERIC_SRC=mem_share.h string.h filereader.h filewriter.h bitvec.h bit2vec.h bitsvec.h hashset.h sort.h list.h dna.h thread.h

PROGS=kbm.huge wtdbg2.huge wtdbg-cns wtpoa-cns

all: $(PROGS)

kbm.huge: $(GENERIC_SRC) kbm.c kbm.h
	$(CC) $(CFLAGS) -o $@ kbm.c $(GLIBS)

wtdbg2.huge: $(GENERIC_SRC) wtdbg.c kbm.h
	$(CC) $(CFLAGS) -o $@ wtdbg.c $(GLIBS)

wtdbg-cns: $(GENERIC_SRC) wtdbg-cns.c kswx.h ksw.h ksw.c dbgcns.h dagcns.h queue.h general_graph.h
	$(CC) $(CFLAGS) -o wtdbg-cns wtdbg-cns.c ksw.c $(GLIBS)

wtpoa-cns: $(GENERIC_SRC) wtpoa-cns.c poacns.h tripoa.h ksw.h ksw.c
	$(CC) $(CFLAGS) -o $@ wtpoa-cns.c ksw.c $(GLIBS)

clean:
	rm -f *.o *.gcda *.gcno *.gcov gmon.out $(PROGS)

clear:
	rm -f *.o *.gcda *.gcno *.gcov gmon.out

install:
	cp -fvu $(PROGS) $(BIN)
