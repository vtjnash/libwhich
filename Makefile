default: libwhich

TARGET = $(shell uname)
override CFLAGS += -Wall -pedantic -O2
override LDFLAGS +=

ifeq ($(TARGET),WINNT)
override CFLAGS += -std=c99 -municode -mconsole -ffreestanding -nostdlib \
	-fno-stack-check -fno-stack-protector -mno-stack-arg-probe
override LDFLAGS += -municode -mconsole -ffreestanding -nostdlib \
	--disable-auto-import --disable-runtime-pseudo-reloc \
	-lntdll -lkernel32 -lpsapi
else
override CFLAGS += -std=gnu99 -D_GNU_SOURCE
endif

ifeq ($(TARGET),Linux)
override LDFLAGS += -ldl
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

.PHONY: clean check default
