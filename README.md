# glitch-core

Glitch core library and command-line app

## Reference

Glitch syntax is arithmetic expressions, most likely you still remember it from
the math class.

Arithmetics: `+` `-` `*` `/` `%` (modulo) `**` (power)

Bitwise: `&` `|` `^` (xor or bitwise not) `<<` `>>`

Compare: `==` `!=` `<` `<=` `>` `>=` (return 1 or 0)

Grouping: `(` `)` `,` (separates expressions or function arguments)

Conditional: `&&` `||` (short-circuit operators)

Assignment: `=` (left side must be a variable)

Instruments:

| Function | Description | Example |
|----------|-------------|---------|
| sin(freq) | sine wave at given frequency | sin(440) |
| tri(freq) | triangular wave at given frequency | tri(440) |
| saw(freq) | saw-tooth wave at given frequency | saw(440) |
| sqr(freq, [pwm=0.5]) | square wave at given frequency and (optionally) pwm | sqr(440) |
| fm(freq, [v1, m1, v2, m2, v3, m3]) | FM-synthesizer with 3 operators, vN is operator strength, mN is operator multiplier, operators 1 and 2 are sequential, operator 3 is parallel to operator 1 | fm(440, 0.5, 0.5) |
| tr808(drum, [vol=1]) | plays TR808 drum sample at given volume, drum 0 is kick, 1 is snare | tr808(0) |

Sequencers:

| Function | Description | Example |
|----------|-------------|---------|
|a(i, ...) | array element by its index, most primitive sequencer | sin(a(t>>10, 440, 466, 493)) |
|seq(bpm, ...) | switches elements at given tempo | sin(loop(120, 400, 466, 493)) |
|loop(bpm, ...) | switches elements at given tempo, unlike seq() it evaluates each argument for each time frame which makes it possible to nest loops | sin(loop(60, seq(240,400,466,493), seq(480, 400,493))) |

Utils:

| Function | Description | Example |
|----------|-------------|---------|
| r(n) | random number in the range [0..n), it sounds like white noise, good for synthesizing drums or making randomized music patterns| r(100) |
| s(phase)  | sine wave amplitude at given phase | s(t*14) |
| l(x) | binary logarith, useful to convert frequencies to note values | note=l(440)*12 |
| hz(note) | note frequency of the given note index, index 0 is note A of 4th octave | s(t*hz(0)*255/8000) |
| scale(pos, mode) | return note index at given position in given scale, scale 0 is major scale, scale 6 is minor | sin(hz(scale(t>>11&7))) |
| mix(...) | mixes voices together | mix(sin(220), sin(440), tri(880)) |
| lpf(voice, cutoff) | applies low-pass filter to the voice at given cutoff frequency | lpf(saw(440)) |
