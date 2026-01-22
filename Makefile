CC=gcc
CFLAGS= -std=gnu99 -Wall -Wextra -Werror -pedantic -pthread -lrt

.PHONY: all clean zip

all: proj2

proj2: proj2.c
	$(CC) $(CFLAGS) $^ -o $@


clean:
	rm -f *.o *.zip proj2

zip:
	zip proj2.zip *.c Makefile