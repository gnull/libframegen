.PHONY: all clean

CFLAGS += -Wall -g

src = $(wildcard *.c)
obj = $(src:.c=.o)
dep = $(src:.c=.d)

all: $(dep) lib.a lib.so

%.d: %.c
	$(CC) $(CFLAGS) -MM $^ -o $@

include $(dep)

lib.a: $(obj)
	ar rcs $@ $?

lib.so: $(obj)
	ld -shared -o $@ $^

clean:
	@rm -vf *.d *.o *.a *.so