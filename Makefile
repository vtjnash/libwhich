default: libwhich

TARGET = $(shell uname)
override CFLAGS += -Wall -pedantic -O2
override LDFLAGS +=

ifeq ($(TARGET),WINNT)
override CFLAGS += -std=c99 -municode
override LDFLAGS += -municode -lpsapi
else
override CFLAGS += -std=gnu99 -D_GNU_SOURCE
endif

ifeq ($(TARGET),Linux)
override LDFLAGS += -ldl
endif

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	-rm libwhich libwhich.o

check: libwhich
	./test-libwhich.sh

.PHONY: clean check default
