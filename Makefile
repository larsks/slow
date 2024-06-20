prefix = /usr/local
bindir = $(prefix)/bin

INSTALL = install
OBJS = slow.o buffer.o term.o
DEPS = $(OBJS:.o=.d)
CFLAGS = -g

%.d: %.c
	$(CC) -MM -o $@ $<

all: slow

slow: $(OBJS)
	$(CC) -o $@ $(OBJS)

test_buffer: buffer.o test_buffer.o
	$(CC) -o $@ $^

install: all
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 slow $(DESTDIR)$(bindir)
	
clean:
	rm -f slow test_buffer *.o *.d

include $(DEPS)
