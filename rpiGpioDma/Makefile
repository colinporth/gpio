# Overwrite PI-version just when compiling:
# $ PI_VERSION=1 make
PI_VERSION ?= 3

CFLAGS=-O3 -W -Wall -std=c99 -D_XOPEN_SOURCE=500 -g -DPI_VERSION=$(PI_VERSION)

test: gpio-dma-test.o mailbox.o

clean:
	rm -f test *.o
