SCRAP_VERSION := 0.1.1-beta

CC = emcc
OUTPUT_DIR = build/

all : $(OUTPUT_DIR)tinyfd.o
	$(CC) -o $(OUTPUT_DIR)scrap.html scrap.c -Os $(RAYLIB_DIR)libraylib.a $(OUTPUT_DIR)tinyfd.o -I. -I$(RAYLIB_DIR) -L. -L$(RAYLIB_DIR) -s USE_GLFW=3 --shell-file shell.html -DPLATFORM_WEB -DSCRAP_VERSION=\"0.1.1-beta-web\" --preload-file data/ -pthread

$(OUTPUT_DIR)tinyfd.o : external/tinyfiledialogs.c external/tinyfiledialogs.h
	$(CC) -o $(OUTPUT_DIR)tinyfd.o -c external/tinyfiledialogs.c