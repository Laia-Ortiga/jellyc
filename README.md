# Jelly

A programming language that is small, simple and fast.

## Development

Jelly is currently highly experimental and not suitable for professional projects.

## Building from Source

The following dependencies are required:
 * CMake >= 3.25
 * C compiler that supports C23
 * OpenMP (optional)

## Usage

There are two backends available: C and LLVM IR.
Both backends will output a single file with name "a" and the appropiate extension.
For instance, a project can be compiled following these commands:

```
jellyc -backend=llvm main.jel util.jel
clang a.ll
```

## Examples

The repository contains some test projects that can be useful to learn how Jelly code looks like.
