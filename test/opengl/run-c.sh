#!/usr/bin/env bash
cd "$(dirname "$0")"
../../build/jellyc *.jel ../../lib/libc.jel
[ $? -eq 0 ] || exit 1
clang a.c glad/src/gl.c -Iglad/include -lm -lglfw && ./a.out