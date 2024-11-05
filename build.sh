#!/bin/sh

set -xe

mkdir -p bin
gcc -O2 -s -o bin/scrap src/*.c -lraylib
