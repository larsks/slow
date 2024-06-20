prefix = /usr/local
bindir = $(prefix)/bin

INSTALL = install
OBJS = slow.o buffer.o term.o
CFLAGS = -g

all: slow

slow: $(OBJS)
	$(CC) -o $@ $(OBJS)

test_buffer: buffer.o test_buffer.o
	$(CC) -o $@ $^

install: all
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 slow $(DESTDIR)$(bindir)
	
clean:
	rm -f slow test_buffer *.o
