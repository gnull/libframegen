.PHONY: run

target = $(shell basename `pwd`)

CFLAGS += -lframegen -lrfc2544 -g

ifdef PREFIX
CFLAGS += -L$(PREFIX)/lib -I$(PREFIX)/include
endif

$(target): *.c
	$(CC) $(CFLAGS) -o $@ $^

test: $(target)
	sudo ./rfc2544-default.sh

