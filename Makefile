CC := gcc
CFLAGS := -Wall -Wextra -O0 -g -DDEBUG
LDFLAGS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
OBJFILES := vec.o scrap.o
EXE_NAME := scrap

all: $(EXE_NAME)

clean:
	rm $(OBJFILES) $(EXE_NAME)

run: $(EXE_NAME)
	./$(EXE_NAME)

$(EXE_NAME): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

vec.o: external/vec.c
	$(CC) $(CFLAGS) -c -o $@ $^

scrap.o: scrap.c
	$(CC) $(CFLAGS) -c -o $@ $^
