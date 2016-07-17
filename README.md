# glitch-core

Glitch core library and command-line app

## Build

On linux: `make alsa=1` or `make alsa=1 pulse=1`. If you want to use JACK: `make jack=1`.

On windows: `make windows=1`.

On MacOS: `make macos=1`.

## Reference

Glitch syntax is arithmetic expressions, most likely you still remember it from
the math class.

Arithmetics: `+` `-` `*` `/` `%` (modulo) `**` (power)

Bitwise: `&` `|` `^` (xor or bitwise not) `<<` `>>`

Compare: `==` `!=` `<` `<=` `>` `>=` (return 1 or 0)

Grouping: `(` `)` `,` (separates expressions or function arguments)

Conditional: `&&` `||` (short-circuit operators)

Assignment: `=` (left side must be a variable)

