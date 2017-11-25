default: libwhich

override CFLAGS += -std=gnu99 -D_GNU_SOURCE
override LDFLAGS += -ldl

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)

clean:
	-rm libwhich libwhich.o

check: libwhich
	./test-libwhich.sh

.PHONY: clean check default
