# only set HOST for the target platform when cross compiling, with f.e. mingw
#HOST	:= i686-w64-mingw32

# path to static libs
LIBDIR=/usr/local/lib

### nothing to edit below ###

ifneq ($(HOST),)
CC	:= $(HOST)-gcc
else
CC	:= gcc
HOST	:= $(shell uname -s)_$(shell uname -m)
endif

CFLAGS  += -O -Wall
LDFLAGS += -framework IOKit -framework CoreFoundation	# for OS X

OBJ := easp.o

all: easp

easp:	$(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBDIR)/libftdi.a $(LIBDIR)/libusb.a $(LIBDIR)/libusb-1.0.a

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) easp easp-*

dist:
	tar cjvf ../easp-$(shell date --rfc-3339=date).tar.bz2 --numeric-owner \
	    --exclude=".hg*" --exclude="*.svf" ../$(shell echo "$${PWD##/*/}")
dist-clean: clean dist
