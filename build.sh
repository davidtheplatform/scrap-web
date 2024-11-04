#!/bin/sh

set -xe

gcc -O2 -o scrap main.c vec.c -I ./raylib/include -L ./raylib/lib ./raylib/lib/libraylib.a -lm
