default: libwhich

override CFLAGS += -std=gnu99 -D_GNU_SOURCE
override LDFLAGS += -ldl

libwhich: libwhich.o
	$(CC) $< -o $@ $(LDFLAGS)
