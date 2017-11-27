default: libwhich

override CFLAGS += -std=gnu99 -D_GNU_SOURCE
override LDFLAGS +=
ifeq ($(shell uname),Linux)
override LDFLAGS += -ldl
endif

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	-rm libwhich libwhich.o

check: libwhich
	./test-libwhich.sh

.PHONY: clean check default
