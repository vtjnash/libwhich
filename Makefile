default: libwhich

override CFLAGS += -std=gnu99 -D_GNU_SOURCE
ifeq (,$(findstring $(shell uname),FreeBSD OpenBSD NetBSD DragonFly))
# BSD systems include libdl as part of libc
override LDFLAGS += -ldl
endif

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	-rm libwhich libwhich.o

check: libwhich
	./test-libwhich.sh

.PHONY: clean check default
