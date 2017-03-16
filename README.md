# Glitch

Glitch is a minimal environment for creating algorithmic music and live coding.

It uses arithmetic expressions to synthesize instruments and create music patterns.

Try it online: http://naivesound.com/glitch

Download for Mac, Windows or Linux: https://github.com/naivesound/glitch/releases

Read more about Glitch on Medium: https://medium.com/@naive_sound

## Build

On linux: `make alsa=1` or `make alsa=1 pulse=1`. If you want to use JACK: `make jack=1`.

On windows: `make windows=1`.

On MacOS: `make macos=1`.

Asm.js: `make js` (requires Docker).

## Reference

Glitch syntax is arithmetic expressions, most likely you still remember it from
the math class.

Arithmetics: `+` `-` `*` `/` `%` (modulo) `**` (power)

Bitwise: `&` `|` `^` (xor or bitwise not) `<<` `>>`

Compare: `==` `!=` `<` `<=` `>` `>=` (return 1 or 0)

Grouping: `(` `)` `,` (separates expressions or function arguments)

Conditional: `&&` `||` ([short-circuit operators][shortcircuit])

Assignment: `=` (left side must be a variable)

### Instruments

| Function | Description | Example |
|----------|-------------|---------|
| sin(freq) | sine wave at given frequency | `sin(440)` |
| tri(freq) | triangular wave at given frequency | `tri(440)` |
| saw(freq) | saw-tooth wave at given frequency | `saw(440)` |
| sqr(freq, [pwm=0.5]) | square wave at given frequency and (optionally) pwm | `sqr(440)` |
| fm(freq, [m1, v1, m2, v2, m3, v3]) | FM-synthesizer with 3 operators, vN is operator strength, mN is operator multiplier, operators 1 and 2 are parallel, operator 3 is sequential to operator 1 | `fm(440, 0.5, 0.5)` |
| tr808(drum, [vol=1], [shift=0]) | plays TR808 drum sample at given volume and pitch shift. The following drum IDs may be used: BD (bass drum), SD (snare drum), MT (middle tom), MA (maracas), RS (rimshot), CP (clap), CB (cowbell), OH (open hat), HH (hi-hat) | `tr808(BD, 1)` |
| piano(freq) | very basic piano sample at the given frequency | `piano(440)`
| pluck(freq, damp, fill) | Karplus-Strong string synthesizer, fill is a function used to prepare the initial values in the delay buffer | `pluck(440, 0.7)` |

FM synthesizer, TR808 sampler and Piano are reset if any of the parameters is NAN. All
instruments return NAN if the input is NAN.

All instruments return a sound wave with the given frequency in the range
[-1..1], so you can combine them by adding the signals (e.g.
`(sin(440)+sin(220))/2`) or modulate using multiplication, e.g.
`saw(440)*sin(1)`.

You may put custom samples into the `samples` subdirectory, each group of
samples should be in a separate folder. Then you could use samples providing
the directory name as a function. For example if you have
`samples/bass/bass0.wav` and `samples/bass/bass1.wav` you may call them as
`bass(0)` and `bass(1)` respectively. Samples are expected to be in the WAV
mono 16-bit format with 44100 Hz sample rate.

### Sequencers:

| Function | Description | Example |
|----------|-------------|---------|
|a(i, ...) | array element by its index, most primitive sequencer | `sin(a(t>>10, 440, 466, 493))` |
|seq(tempo, ...) | switches elements at given tempo | `sin(loop(120, 400, 466, 493))` |
|loop(tempo, ...) | switches elements at given tempo, unlike seq() it evaluates each argument for each time frame which makes it possible to nest loops | `sin(loop(60, seq(240,400,466,493), seq(480, 400,493)))` |

Tempo can be a single number (beats per minute) or a pair `(offset, bpm)`, where
offset is number of beats to skip before starting the sequence.

Seq and loop values can be pairs, too. Then the first value is a relative beat
duration and the second is the actual returned value: `seq(120, (3/4, 1), (1/4,
1))` returns the value of "1", but the first value lasts 3 times longer than
the second value.

If seq takes more values, they will be sliding from one another, e.g.
`seq(120, (1, 0, 4, 2), (1, 2, 4, 0))` slides the values like
`0->4->2->2->4->0`. The first value in a group is still a relative beat
duration.

Seq and loop return NAN every then the value is changed.

### Utils

| Function | Description | Example |
|----------|-------------|---------|
| r(max) | random number in the range [0..max), it sounds like white noise, good for synthesizing drums or making randomized music patterns | `r(100)` |
| s(phase)  | sine wave amplitude at the given phase, unline sin() you must provide phase in the range [0..1] | `s(t*14)` |
| l(x) | binary logarithm, useful to convert frequencies to note values | `note=l(440)*12` |
| hz(note) | note frequency of the given note index, index 0 is note A of 4th octave, you may also use helper variables like `A#4`, `C2`, `Db3` | `sin(hz(A4))` |
| scale(pos, mode) | return note index at given position in given scale, scale 0 is major scale, scale 6 is minor | `sin(hz(scale(t>>11&7)))` |
| env(signal, (dt, level)...) | Creates an ADSR envelope for the signal, envelope is reset if signal is NAN, if first part has non-zero level - the initial level starts from 1, otherwise from 0; if last argument is not zero - the release section is inserted automatically | `env(v, (0.1, 0.2))` |
| mix(...) | mixes voices together, each parameter is a signal or a pair of (volume, signal). Signals are clipped if overflow occurs | `mix(sin(220), sin(440), tri(880))` |
| lpf(voice, cutoff) | applies low-pass filter to the voice at given cutoff frequency | `lpf(v, 200)` |
| hpf(voice, cutoff) | applies high-pass filter to the voice at given cutoff frequency | `hpf(v, 400)` |
| bpf(voice, cutoff) | applies band-pass filter to the voice at given cutoff frequency | `bpf(v, 400)` |
| bsf(voice, cutoff) | applies band-stop filter to the voice at given cutoff frequency | `bsf(v, 400)` |
| delay(voice, time, level, feedback) | delays signal by given time, delay level can be controlled as well as the amount of delay feedback, which affect the number of delay repetitions | `delay(v, 0.1, 0.5, 0.2)` |

### Macros

To reuse the same expression multiple times you may create a macro:

```
$(organ, (sin($1)+0.4*sin($1+7)+0.3*sin($1-5))/3)
(organ(hz(C4))+organ(hz(G4)))/2
```

Macros are definde using the `$(name, body)` function. Body can consist of multiple expressions if you extra parenthesis, e.g. `$(filter, (z=saw($1), lpf(z)))`.

There are special argument variables $1..$9 that get expanded to the actual values when the macro is called.

## Special variables:

`t` is time variable that increases at rate 8000/second.

`x` and `y` in the web version are mouse cursor position, normalized to (0..1) range.

`bpm` is a tempo, user input is synchronized with the playback at this rate, so
you might want to use `bpm=120/4` to synchronize user input every 4 beats.

### Polyphony

To apply the same expression to a number of variables you may use `each()` function. It takes a list of formal variables, a function, and a list of actual values: `each(f, sin(f), 440, 880, 220)`.

You may pass multiple variables as well: `each((vol, freq), vol*sin(freq), (1, 440), (0.4, 880), (0.2, 220))`.

This is useful for live MIDI input.

### MIDI

Glitch provides special variables that change their values if a MIDI keyboard is used:

* k0, k1, ..., k9 - MIDI key values (0=A4, 1=A#4 etc)
* v0, v1, ..., v9 - MIDI velocity (it gradually fades out on key release)
* g0, g1, ..., g9 - MIDI gate signal (it is set to NAN immediately on key release)

Here's how you can play a sine wave with a MIDI keyboard (handling up to 5 keys pressed at a time, use k5, k6 etc to get more polyphony):

```
each((k, v), v*sin(hz(k)),
  (k0, v0), (k1, v1), (k2, v2), (k3, v3), (k4, v4))
```

Special variables `x` and `y` are set to the pitch wheel and modulation wheel values if a MIDI keyboard is used.



[shortcircuit]: https://en.wikipedia.org/wiki/Short-circuit_evaluation
