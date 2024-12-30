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
LINUX_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-linux
WINDOWS_DIR := $(EXE_NAME)-v$(SCRAP_VERSION)-windows64

all: $(EXE_NAME)

clean:
	rm -f $(OBJFILES) $(EXE_NAME) $(EXE_NAME).exe Scrap-x86_64.AppImage $(LINUX_DIR).tar.gz $(WINDOWS_DIR).zip

windows-build: $(EXE_NAME)
	mkdir -p $(WINDOWS_DIR)
	cp -r data examples extras LICENSE README.md $(EXE_NAME).exe $(WINDOWS_DIR)
	zip -r $(WINDOWS_DIR).zip $(WINDOWS_DIR)
	rm -r $(WINDOWS_DIR)

linux-build: $(EXE_NAME)
	mkdir -p $(LINUX_DIR)
	cp -r data examples extras LICENSE README.md $(EXE_NAME) $(LINUX_DIR)
	tar czvf $(LINUX_DIR).tar.gz $(LINUX_DIR)
	rm -r $(LINUX_DIR)

appimage: $(EXE_NAME)
	mkdir -p scrap.AppDir
	cp $(EXE_NAME) scrap.AppDir/AppRun
	cp -r data scrap.desktop extras/scrap.png scrap.AppDir
	appimagetool-x86_64.AppImage scrap.AppDir
	rm -r scrap.AppDir

$(EXE_NAME): $(OBJFILES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

scrap.o: scrap.c external/raylib-nuklear.h vm.h
	$(CC) $(CFLAGS) -c -o $@ scrap.c

filedialogs.o: external/tinyfiledialogs.c
	$(CC) $(CFLAGS) -c -o $@ external/tinyfiledialogs.c
