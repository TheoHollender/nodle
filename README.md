# Nodle

Nodle is a small language for describing graphs and
stating and proving properties about them. This repository contains its compiler,
written in C++20.

## Requirements

- A C++20 compiler (tested with `g++` 13.3)
- GNU `make`
- [GoogleTest](https://github.com/google/googletest), only needed for the
  tests (`sudo apt install libgtest-dev` on Ubuntu/Debian)
- `clang-format`, only needed for `./lint.sh`

The argument parser ([argparse](https://github.com/p-ranav/argparse)) is
vendored in `include/argparse/`, so it does not need to be installed.

If you use Nix, `nix develop` opens a shell with all of the above.

## Building and running

```sh
make                       # build ./build/nodle and the test runner
make build/nodle           # build only the compiler (no GoogleTest needed)
make test                  # build and run all unit tests
make DEBUG=1               # debug build with AddressSanitizer + UBSan
make clean                 # delete the build directory
./lint.sh                  # format the code (./lint.sh --check to only check)
```

Run the compiler on one or more `.nodle` files:

```sh
./build/nodle examples/hw/HelloWorld.nodle
./build/nodle --debug-lexer examples/hw/HelloWorld.nodle        # print the token stream
./build/nodle -Dl --verbose-lexer-source examples/hw/HelloWorld.nodle  # also print source locations
./build/nodle --help
```

For now, the command-line tool only runs the lexer. Without `--debug-lexer` it
prints nothing and exits with status 0 if the file was read successfully.

With Nix, `nix build` builds the compiler into `./result/bin/nodle`,
`nix run . -- <files>` runs it, and `nix flake check` runs the tests. CI runs
the lint check and the tests on every push.

## Code layout

| Path | Contents |
| --- | --- |
| `src/nodle/main.cpp`, `include/nodle/cli/` | Command-line driver and the debug flags |
| `src/nodle/io/` | Source files, source locations, error-highlighting printer |
| `src/nodle/lexer/` | Rule-based lexer and token definitions |
| `include/nodle/ast/ast.hpp` | AST for the whole language |
| `include/nodle/parser/rules/` | Parser-combinator library |
| `src/nodle/parser/parser.cpp` | The Nodle grammar, written with the combinators |
| `tests/` | GoogleTest unit tests, mirroring `src/` |
| `examples/` | Example Nodle programs |
