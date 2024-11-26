#!/bin/sh

set -xe

mkdir -p bin
gcc -Wall -Wextra -O0 -ggdb -DDEBUG -o bin/scrap src/*.c -lraylib
