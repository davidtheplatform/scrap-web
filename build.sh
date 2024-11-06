#!/bin/sh

set -xe

mkdir -p bin
gcc -O2 -DDEBUG -o bin/scrap src/*.c -lraylib
