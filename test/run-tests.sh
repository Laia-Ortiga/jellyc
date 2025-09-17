#!/usr/bin/env bash
build/jellyc -print-debug test/print_type.jel
[ $? -eq 0 ] || echo "tests failed"