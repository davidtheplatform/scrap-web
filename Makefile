CC := gcc
CFLAGS := -Wall -Wextra -O0 -g -DDEBUG
LDFLAGS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
OBJFILES := vec.o scrap.o

all: scrap

clean:
	rm $(OBJFILES) scrap

run: scrap
	./scrap

scrap: $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

vec.o: vec.c
	$(CC) $(CFLAGS) -c -o $@ $^

scrap.o: scrap.c
	$(CC) $(CFLAGS) -c -o $@ $^
