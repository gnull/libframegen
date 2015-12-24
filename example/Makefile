ifdef PREFIX
CFLAGS += -I$(PREFIX)/include -L$(PREFIX)/lib
endif

CFLAGS += -Wall -g -lframegen -lrfc2544 -lethrate -ldbg

main: main.o
	$(CC) $(CFLAGS) -o $@ $^