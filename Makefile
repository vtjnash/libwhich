default: libwhich

prefix := /usr
bindir := $(prefix)/bin

INSTALL := install

TARGET = $(shell uname)
override CFLAGS := -Wall -pedantic -O2 $(CFLAGS)
override LDFLAGS := $(LDFLAGS)

ifeq ($(TARGET),WINNT)
override CFLAGS := -std=c99 -municode -mconsole -ffreestanding -nostdlib \
	-fno-stack-check -fno-stack-protector -mno-stack-arg-probe \
	$(CFLAGS)
override LDFLAGS := -municode -mconsole -ffreestanding -nostdlib \
	--disable-auto-import --disable-runtime-pseudo-reloc \
	-lntdll -lkernel32 \
	$(LDFLAGS)
else
override CFLAGS := -std=gnu99 -D_GNU_SOURCE $(CFLAGS)
endif

ifeq ($(TARGET),Linux)
override LDFLAGS := -ldl $(LDFLAGS)
endif

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)


libz.so:
	$(CC) -o $@ -shared -x c /dev/null
ifeq ($(TARGET),WINNT)
check: libz.so
endif

clean:
	-rm libwhich libwhich.o libz.so

check: libwhich
	./test-libwhich.sh

install: libwhich
	mkdir -p $(DESTDIR)$(bindir)
	$(INSTALL) -m755 libwhich $(DESTDIR)$(bindir)/libwhich

.PHONY: clean check default install
