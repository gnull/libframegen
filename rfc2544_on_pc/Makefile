.PHONY: run

target = $(shell basename `pwd`)

CFLAGS += -lframegen -lrfc2544 -g

ifdef PREFIX
CFLAGS += -L$(PREFIX)/lib -I$(PREFIX)/include
endif

$(target): *.c
	$(CC) $(CFLAGS) -o $@ $^

test: $(target)
	./$(target) --tx_if lo --rx_if lo --conf default.conf --ipsrc 1.1.1.1 --ipdst 2.2.2.2 --macsrc 33:33:33:33:33:33 --macdst 44:44:44:44:44:44 --udpsrc 10 --udpdst 20

