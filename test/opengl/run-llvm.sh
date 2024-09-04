#!/usr/bin/env bash
cd "$(dirname "$0")"
../../build/jellyc -backend=llvm *.jel ../../lib/libc.jel
clang a.ll glad/src/gl.c -Iglad/include -lm -lglfw && ./a.out