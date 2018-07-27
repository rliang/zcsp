PREFIX ?= /usr/local
INCDIR ?= $(PREFIX)/include/zcsp
LIBDIR ?= $(PREFIX)/lib

CFLAGS ?= -Wall -Wextra
CFLAGS += -fPIC -MMD

.PHONY: all install clean

all: libzcsp.so libzcsp.a

libzcsp.so: cr.o ch.o
	$(LD) -shared -o $@ $^

libzcsp.a: cr.o ch.o
	$(AR) rc $@ $^

install: libzcsp.a libzcsp.so zcsp.h
	install -D -d $(INCDIR) zcsp.h
	install -D -d $(LIBDIR) libzcsp.a libzcsp.so

clean:
	rm *.a *.so *.d *.o

-include cr.d ch.d
