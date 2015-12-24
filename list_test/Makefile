ifdef PREFIX
CFLAGS += -I$(PREFIX)/include -L$(PREFIX)/lib
endif

CFLAGS += -Wall -g -lframegen

main: main.o
	$(CC) $(CFLAGS) -o $@ $^