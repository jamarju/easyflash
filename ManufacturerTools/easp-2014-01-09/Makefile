# only set HOST for the target platform when cross compiling, with f.e. mingw
#HOST	:= i686-w64-mingw32

host_os := $(shell uname -s)
LIBDIR ?= /usr/local/lib

### nothing to edit below ###

ifneq ($(HOST),)
CC := $(HOST)-gcc
FTDI_LIBS := $(LIBDIR)/libftdi.a $(LIBDIR)/libusb.a $(LIBDIR)/libusb-1.0.a
else
CC := gcc
HOST := $(host_os)_$(shell uname -m)
ifeq ($(host_os),Darwin)
LIBFTDI_PREFIX ?= $(shell brew --prefix libftdi 2>/dev/null)
LIBUSB_PREFIX ?= $(shell brew --prefix libusb 2>/dev/null)
CPPFLAGS += -I$(LIBFTDI_PREFIX)/include/libftdi1 -I$(LIBUSB_PREFIX)/include/libusb-1.0
LDFLAGS += -framework IOKit -framework CoreFoundation
LDFLAGS += -framework Security
STATIC ?= yes
ifeq ($(STATIC),yes)
FTDI_LIBS := $(LIBFTDI_PREFIX)/lib/libftdi1.a $(LIBUSB_PREFIX)/lib/libusb-1.0.a
else
FTDI_LIBS := -L$(LIBFTDI_PREFIX)/lib -lftdi1 -L$(LIBUSB_PREFIX)/lib -lusb-1.0
endif
else
FTDI_LIBS := $(LIBDIR)/libftdi.a $(LIBDIR)/libusb.a $(LIBDIR)/libusb-1.0.a
endif
endif

CFLAGS  += -O -Wall

OBJ := easp.o

all: easp

easp:	$(OBJ)
	@if [ "$(host_os)" = Darwin ] && [ "$(STATIC)" = yes ]; then \
		test -n "$(LIBFTDI_PREFIX)" || { echo "Homebrew libftdi is missing; install it"; exit 1; }; \
		test -n "$(LIBUSB_PREFIX)" || { echo "Homebrew libusb is missing; install it"; exit 1; }; \
		test -f "$(LIBFTDI_PREFIX)/lib/libftdi1.a" || { echo "Homebrew libftdi static archive is missing; install libftdi"; exit 1; }; \
		test -f "$(LIBUSB_PREFIX)/lib/libusb-1.0.a" || { echo "Homebrew libusb static archive is missing; install libusb"; exit 1; }; \
	fi
	$(CC) $(LDFLAGS) -o $@ $^ $(FTDI_LIBS)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) easp easp-*

dist:
	tar cjvf ../easp-$(shell date --rfc-3339=date).tar.bz2 --numeric-owner \
	    --exclude=".hg*" --exclude="*.svf" ../$(shell echo "$${PWD##/*/}")
dist-clean: clean dist
