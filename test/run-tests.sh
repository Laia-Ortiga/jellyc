#!/usr/bin/env bash

success_counter=0
failure_counter=0

success() {
    echo -e "\033[1;32m[success]\033[0m $@"
    success_counter=$((success_counter + 1))
}

failure() {
    echo -e "\033[1;31m[failed]\033[0m $@"
    failure_counter=$((failure_counter + 1))
}

test() {
    eval "($@) &> /dev/null"
    [ $? -eq 0 ] && success "$@" || failure "$@"
}

test_c() {
    test "jellyc -backend=c $@ && gcc a.c -lm"
}

test_llvm() {
    test "jellyc -backend=llvm $@ && clang a.ll -lm"
}

test_all() {
    test_c "$@"
    test_llvm "$@"
}

export PATH=build:$PATH

test_all test/hello_world.jel
test_all lib/*.jel test/array.jel
test_all lib/*.jel test/basic_lexer.jel
test_all lib/*.jel test/fibonacci.jel
test jellyc -backend=c    lib/*.jel test/opengl/*.jel
test jellyc -backend=llvm lib/*.jel test/opengl/*.jel
test jellyc -print-debug test/print_type.jel
test_all lib/*.jel test/feature/if.jel
test_all lib/*.jel test/feature/switch.jel
test_all lib/*.jel test/feature/while.jel
rm -f a.c a.ll a.out
echo "$success_counter tests passed"
echo "$failure_counter tests failed"
