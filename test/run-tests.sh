#!/usr/bin/env bash

test() {
    eval "$@" &> /dev/null
    [ $? -eq 0 ] && echo -e "\033[1;32m[success]\033[0m $@" || echo -e "\033[1;31m[failed]\033[0m $@"
}

export PATH=build:$PATH

test jellyc -backend=c    test/hello_world.jel
test jellyc -backend=llvm test/hello_world.jel
test jellyc -backend=c    lib/*.jel test/array.jel
test jellyc -backend=llvm lib/*.jel test/array.jel
test jellyc -backend=c    lib/*.jel test/basic_lexer.jel
test jellyc -backend=llvm lib/*.jel test/basic_lexer.jel
test jellyc -backend=c    lib/*.jel test/fibonacci.jel
test jellyc -backend=llvm lib/*.jel test/fibonacci.jel
test jellyc -backend=c    lib/*.jel test/opengl/*.jel
test jellyc -backend=llvm lib/*.jel test/opengl/*.jel
test jellyc -print-debug test/print_type.jel
rm -f a.c a.ll
