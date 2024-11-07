#!/bin/sh

set -xe

mkdir -p bin
gcc -Wall -Wextra -O2 -DDEBUG -o bin/scrap src/*.c -lraylib
