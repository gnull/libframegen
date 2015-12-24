.PHONY: all install clean

name = libframegen

ifdef PREFIX
CFLAGS += -L$(PREFIX)/lib -I$(PREFIX)/include
endif

CFLAGS += -Wall -g -fpic -lethrate -lrfc2544 -ldbg

src = $(wildcard *.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)

all: $(dep) $(name).a $(name).so

ifdef PREFIX
install: all
	@cp -vf $(name).so $(name).a $(PREFIX)/lib/
	@cp -vf export.h $(PREFIX)/include/$(name).h
endif

%.d: %.c
	$(CC) $(CFLAGS) -MM $^ -o $@

include $(dep)

$(name).a: $(obj)
	ar rcs $@ $?

$(name).so: $(obj)
	$(CC) $(CFLAGS) -shared -o $@ $^

clean:
	@rm -vf *.d *.o *.a *.so