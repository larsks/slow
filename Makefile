prefix = /usr/local
bindir = $(prefix)/bin

INSTALL = install
OBJS = slow.o

all: slow

slow: $(OBJS)
	$(CC) -o $@ $(OBJS)

install: all
	$(INSTALL) -m 755 -d $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 slow $(DESTDIR)$(bindir)
