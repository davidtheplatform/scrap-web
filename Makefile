SCRAP_VERSION := 0.1-beta

TARGET ?= LINUX

ifeq ($(TARGET), LINUX)
	CC := gcc
	CFLAGS := -Wall -Wextra -O3 -s -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -fmax-errors=5
	LDFLAGS := -lraylib -lGL -lm -lpthread -lX11
else
	CC := x86_64-w64-mingw32-gcc
	CFLAGS := -Wall -Wextra -O3 -s -DSCRAP_VERSION=\"$(SCRAP_VERSION)\" -fmax-errors=5 -I./raylib/include -L./raylib/lib
	LDFLAGS := -static -lraylib -lole32 -lcomdlg32 -lwinmm -lgdi32 -Wl,--subsystem,windows
endif

OBJFILES := scrap.o filedialogs.o
EXE_NAME := scrap

all: $(EXE_NAME)

clean:
	rm -f $(OBJFILES) $(EXE_NAME) $(EXE_NAME).exe

$(EXE_NAME): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

scrap.o: scrap.c external/raylib-nuklear.h vm.h
	$(CC) $(CFLAGS) -c -o $@ scrap.c

filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ external/tinyfiledialogs.c
