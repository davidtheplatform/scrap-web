CC := gcc
CFLAGS := -Wall -Wextra -O0 -ggdb -DDEBUG
LDFLAGS := -lraylib
OBJFILES := vec.o scrap.o

all: scrap

clean:
	rm $(OBJFILES) scrap

run: scrap
	./scrap

scrap: $(OBJFILES)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

vec.o: vec.c
	$(CC) $(CFLAGS) -c -o $@ $^

scrap.o: scrap.c
	$(CC) $(CFLAGS) -c -o $@ $^
