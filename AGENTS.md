# bloop: notes for agents

bloop is a small, strict, JavaScript-like scripting language for embedding in C++ programs (C++20).

## Before writing bloop code

bloop looks like JavaScript but isn't: no `null`, `this`, `var`, `class`, `switch`, `??`, `?.`, spread or default
parameters; `x => x` needs parentheses (`(x) => x`); strings and arrays have no methods (`std.length(a)`,
`std.array.push(a, x)`); only booleans, numbers and undefined can be conditions; `"n" + 1` is an error; a name can't
be declared again while it's visible (no shadowing); `7 / 2` is `3`. Read these first:

- `docs/html/llms-short.txt`: the pitfalls with the exact error each one gives, and every standard library signature
- `docs/html/llms.txt`: the whole language reference as one plain text file
- `docs/src/`: the same reference as plain text pages (`agents.txt` is the pitfalls page)

Run a script with the interpreter: `bloop file.bloop [arguments...]`. Errors print their line and column.

## Layout

- `src/`: the library (`bloop_lib`) and the interpreter (`bloop`, `src/main.cpp`)
  - `lexer/`, `parser/`, `ast/`, `resolver/`, `bytecode/`: compiling (operator precedence: `lexer/punctuation.hpp`)
  - `vm/`: the virtual machine; `module/`: modules and imports; `embed/` and `bloop.hpp`: the embedding API
  - `std/lib/<name>/`: the standard library modules (std.array, std.string, std.task...)
- `tests/`: Catch2 tests (`bloop_test`). Each area is `tests/vm/<area>/_<area>.cpp` with the `.bloop` files it runs;
  helpers in `tests/vm/defs.hpp` (`CheckConstant`, `CheckArray`, `CheckObject`, `TEST_CompileFile`...)
- `docs/`: the reference. `docs/src/*.txt` are the sources (format in `docs/src/README.txt`), `docs/build.bloop`
  turns them into `docs/html` (and the `llms*.txt` files)
- `examples/embedding/`: embedding from C++

`src/code.def` is the owner's scratch script (the interpreter runs it when given no file): don't overwrite it.

## Building and testing

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target bloop_test
build/tests/Debug/bloop_test            (Windows; build/tests/bloop_test elsewhere)
```

CI (`.github/workflows/tests.yml`) builds and tests on Windows (MSVC) and Linux (gcc, clang), and runs a job with
AddressSanitizer and GC stress. Code must build with all three compilers. Keep each file's line endings as they are.

## After changing the language

- Add a test next to the area's other tests (a `.bloop` file and a `TEST_CASE` in the area's `_<area>.cpp`).
- Update the page in `docs/src/` and rebuild the docs from the repository root: `bloop docs/build.bloop`. Fix every
  warning it prints (a duplicate term, a `[[link]]` that leads nowhere).
- A new JavaScript difference goes in the table in `docs/src/agents.txt`, with its exact error message.
