# Maze Solver + BigInt Expression Evaluator

University assignment. C17.

## Build (must compile clean, no warnings)
gcc -std=c17 -Wall -Wextra -Werror -pedantic -g main.c maze.c expression.c bigint.c -o maze_solver

## Code style
Read `.style-reference/examples.java` before writing code. The examples are Java,
so only the surface conventions transfer, not the structure:
- snake_case, short variable names
- no spaces around `=`, `<`, `>`, `+` in expressions
- opening brace on its own line for blocks; same line for function signatures
- minimal comments
Do not reformat to a generic house style.

## Hard constraints
- Files: main.c maze.c maze.h expression.c expression.h bigint.c bigint.h
- Do NOT change declarations in maze.h or expression.h
- '#' is TRAVERSABLE, ' ' is a WALL (inverted from the usual convention)
- Rows may have different lengths; a column past a row's end is a wall
- Leading/trailing spaces and empty lines in maze files are meaningful
- BigInt: no fixed-size arrays, no MAX_DIGITS/MAX_BITS/MAX_BLOCKS, no external bignum libs
- Never convert a full value to long long/int64_t/uint64_t to perform an operation
- Shift amounts may exceed built-in types; compare against width before casting to size_t
- MAZE_INVALID and EXPR_INVALID are normal results, not errors — never print to stderr for them
- System errors (file open failure, alloc failure, bad argv) → stderr + EXIT_FAILURE
- Must pass: valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1

## Modes
- SIGNED N / UNSIGNED N: keep lowest N bits after every literal and every operation
- UNLIMITED: unsigned arbitrary precision, no wraparound; negative intermediate = EXPR_INVALID
- UNLIMITED width rules are in the spec section 4.3.1 — widths are tracked, not minimized