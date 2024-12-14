CC := gcc
CFLAGS := -Wall -Wextra -O0 -g -fmax-errors=5
LDFLAGS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
OBJFILES := scrap.o
EXE_NAME := scrap

all: $(EXE_NAME)

clean:
	rm $(OBJFILES) $(EXE_NAME)

run: $(EXE_NAME)
	./$(EXE_NAME)

$(EXE_NAME): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

scrap.o: scrap.c external/raylib-nuklear.h vm.h
	$(CC) $(CFLAGS) -c -o $@ scrap.c
