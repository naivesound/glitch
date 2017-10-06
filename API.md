# Glitch API

Glitch is an algorithmic synthesizer. It creates music with math. Below is a
list of functions that you can use in Glitch to generate or modify the sound.

## Input and output

Music can be seen as a function `f(t)` where `t` is increasing in time.

Glitch increases a variable `t` at `8000/sec` rate and it can be a real number if your
hardware sample rate is higher. Expression result is expected to be in range
`[-1..1]` otherwise it will be clipped.

Music expression is evaluated once for each audio frame. You can use numbers,
math operators, variables and functions to compose more complex expressions.

Most of the functions keep track of time internally, so you only have to
specify other arguments such as frequency.

## Math

Everything in Glitch is a number. Time is a number. Notes are represented as
numbers. Every signal is represented as the value of its current amplitude.

That is why music in Glitch is made using the following arithmetic operators:

* Arithmetics: `+` `-` `*` `/` `%` (modulo) `**` (power)
* Bitwise: `&` `|` `^` (xor or bitwise not) `<<` `>>`
* Compare: `==` `!=` `<` `<=` `>` `>=` (return 1 or 0)
* Grouping: `(` `)` `,` (separates expressions or function arguments)
* Conditional: `&&` `||` ([short-circuit operators][short-circuit])
* Assignment: `=` (left side must be a variable)

These arithmetic operators have the same meaning as they have in most
programming languages. The priority of the operators is the one [used in C
language][operator-priority].

Since everything in Glitch is a number, `+` can be used to mix signals
together. `-` can be used to subtract one signal from another.

> Example: `piano+drums` returns two signals (piano and drums) mixed together.

In a similar manner, `*` can be used to modulate signals. 

> Example: `piano*lfo` modulates piano signal with the lfo signal value.

Parenthesis can be used to group subexpressions, but also to pass parameter
tuples instead of single arguments. If a function takes a tuple as an argument
it will be mentioned in the documentation.

Comma operator separate function arguments. Outside of functions comma allows
to evaluate parts of the formula one by one, from left to right. The result of
the last part is returned. Comma is so frequently used in Glitch, that you may
omit commas at the end of line:

```
# Comma at the end of line is optional:
x=(a=1,b=2,a+b)
x = (
  a = 1
	b = 2
	a + b
)
```

> Example: `x=5, y=6, x+y`. X is set to 5, then y is set to 6, then 11 is returned.

> Example: `x=(a=1,a+2). A is set to 1, then 3 is retured and assigned to X.

Short circuit operators (`&&` and `||`) are used to achieve the effect of
if/else statements. `&&` evaluates the rigth side only if the left side is
true. `||` evaluates the right side only if the left side is false.

> Example: `(b && 1 || 0)` returns 1 if b is non-zero (true) and returns 0 if b is zero.

## Math functions

### l(x)

`l(x)` returns a binary logarithm (log2) of `x`. Useful to convert frequencies
to note values. It is rarely used in Glitch.

> Example: `note=l(440) * 12` - returns note value for the frequency 440Hz, which is `A4`, or 0.

### r(max=1)

`r(max)` returns a random number in the range [0..max), it sounds like white
noise, good for synthesizing drums or making randomized music patterns.

`max` parameter is optional and by default `r()` returns numbers in the range `[0..1)`

> Example: `r(100)` - returns a random number in the range [0..100)

### s(phase=0)

`s(phase)` returns a sine wave amplitude for the given phase. Phase must be in the range `[0..1)`. Returned value is in the range `[-1..1]`.

> Example: [x=x+1, s(x*0.006)](http://localhost:8000/#x%3Dx%2B1%2C%20s(x*0.006)) - plays a sine wave at 437Hz

### byte(x=127)

`byte(x)` converts the value `x` from [Bytebeat][bytebeat] range `[0..255]` to
the common amplitude range `[-1..1]`. It is used to play short Bytebeat
formulas in Glitch as well as add effects to them or mix with other sounds.

> Example: [byte(t*5&(t>>7)|t*3&(t*4>>10))](http://localhost:8000/#byte(t*5%26(t%3E%3E7)%7Ct*3%26(t*4%3E%3E10))) - Bytebeat music

## Sequencers

Sequencers are used to describe melodies, chord progressions, rhythmic patterns
or other repetitive parts of your song.

### a(index, values...)

`a(i, x0, x1, x2, ...)` returns x[i] value for the given index i. If index is
negative or out of array bounds - it gets wrapped around.

> Example: `a(0, 5, 6, 7)` - returns 5, because it is at index 0 in the list of values.

> Example: `a(2, 5, 6, 7)` - returns 7

> Example: `a(3, 5, 6, 7)` - returns 5

> Example: `a(4, 5, 6, 7)` - returns 6

> Example: `a(-1, 5, 6, 7)` - returns 7

> Example: [byte(t*a(t>>11,4,5,6))](http://localhost:8000/#byte(t*a(t%3E%3E11%2C4%2C5%2C6))) - plays saw-tooth wave with 3 changing frequencies in a loop

### seq((offset, bpm), (step, values...)...)

`seq(bpm, x0, x1, x2, ...)` returns x[i] value where i is increated at given tempo (`bpm`).

Values can be numeric constants, variables or expressions. Values are evaluated
once per beat and the result is remembered and then reused.

Values can be a tuples. In a pair of numbers like (2,3) the first number is
relative step duration and the second one is the actual value. This means value
3 will be played for 2 beats.

Value can be a tuple of more than 2 numbers. The first number is always a
relative step duration, and the other values are gradually slided, e.g.
(0.5,2,4,2) is a value changed from 2 to 4 back to 2 and the step duration is
half of a beat.

> Example: [byte(t*seq(120,4,5,6))](http://localhost:8000/#byte(t*seq(120%2C4%2C5%2C6)))

> Example [byte(t*seq(120,(1,4,6,4),(1/2,5),(1/2,6)))](http://localhost:8000/#byte(t*seq(120%2C(1%2C4%2C6%2C4)%2C(1%2F2%2C5)%2C(1%2F2%2C6))))

### loop((offset, bpm), (step, expr)...)

`loop(bpm, x0, x1, x2, ...)` evaluates x[i] increasing i at the given tempo.

Unlike `seq()`, `loop()` evaluates x[i] for every audio frame, so other
functions can be used as loop values.

`seq()` is often used to change pitch or volume, `loop()` is often used to
schedule inner sequences and loops.

> Example: [byte(t*loop(30,seq(240,4,5),seq(240,4,6)))](http://localhost:8000/#byte(t*loop(30%2Cseq(240%2C4%2C5)%2Cseq(240%2C4%2C6))))

`seq()` and `loop()` return NaN at the beginning of each step. NaN value is
used by the instruments and effects to detect the start of a new note.

## Instruments

Oscillators are the building blocks of synthesizers. Oscillator phase is
managed internally, only frequency must be provided (in Hz).

### sin(freq)

`sin(freq)` plays a sine wave at the given frequency. If frequency is negative
- sine wave is played "backwards". The start of the wave is always at zero
amplitude.

### tri(freq)

`tri(freq)` plays a trianglular wave. Like `sin()`, is starts at zero value and
plays backwards if frequency is a negative value.

## saw(freq)

`saw(freq)` plays a sawtooth wave. Like `sin()` and `tri()`, is starts at zero
value and plays backwards if frequency is a negative value.

## sqr(freq, pwm=0.5)

`sqr(freq, pwm)` plays a square wave with the given pulse width. By default if
no pulse width is given it is assumed to be 50%, or `0.5`.

> Example: [(sin(220)+tri(440))/2](http://localhost:8000/#(sin(220)%2Btri(440))%2F2)

## fm(freq, mf1=0, ma1=0, mf2=0, ma2=0, mf3=0, ma3=0)

`fm(freq, mf1, ma1, mf2, ma2, mf3, ma3)` is a 3-operator FM synthesizer, `mf`
is an operator frequency ratio, `ma` is an operator amplification. Operators M2
and M1 are parallel, M3 is sequential to M1.

> Example: [fm(seq(120,440,494),1,0.5,0.01,1)](http://localhost:8000/#fm(seq(120%2C440%2C494)%2C1%2C0.5%2C0.01%2C1))

## tr808(instr, volume=1, pitch=0)

`tr808(instr, volume)` is a set of samples from the TR808 drum kit.

The following instruments are supported (you can use two-letter constants
instead of remembering numbers):

* `BD=0` - Bass drum
* `SD=1` - Snare drum
* `MT=2` - Middle tom
* `MA=3` - Maracas
* `RS=4` - Rimshot
* `CP=5` - Clap
* `CB=6` - Cowbell
* `OH=7` - Open hat
* `HH=8` - High hat

> Example: [tr808(SD,seq(240,1,0.2))](http://localhost:8000/#tr808(SD%2Cseq(240%2C1%2C0.2)))

## Effects and signal processing

### lpf(signal, cutoff=200, Q=1)

`lpf(signal, cutoff, Q)` is a bi-quadratic low-pass filter. It passes the
signals with the frequencies lower than the given cutoff frequency and
attenuates the signals with the frequency higher than cutoff. Q is the Q-factor
of the filter.

### hpf(signal, cutoff=200, Q=1)

`hpf(signal, cutoff, Q)` is a bi-quadratic high-pass filter. It passes the
signals with the frequencies higher than the given cutoff frequency and
attenuates the signals with the frequency lower than cutoff. Q is the Q-factor
of the filter.

`hpf()` is complementary to the `lpf()`.

### bpf(signal, cutoff=200, Q=1)

`bpf(signal, cutoff, Q)` is a bi-quadratic band-pass filter. It passes the
signals with the frequencies in the range around the given cutoff frequency and
attenuates other signals. Q is the Q-factor of the filter and defines the width
of the range.

### bsf(signal, cutoff=200, Q=1)

`bsf(signal, cutoff, Q)` is a bi-quadratic band-stop filter. Some variants of
it are known as Notch filter.  It passes the signals with the frequencies
outside of the range around the given cutoff frequency and attenuates the rest
of the signals. Q is the Q-factor of the filter and defines the width of the
range.

### delay(signal, time=0, damp=0, feedback=0)

`delay(signal, time, damp, feedback)` is a simple delay effect. It delays the
incoming signal for the given time in seconds. The signal is damped to the
given level and on further loops of the delay line is amplified by the feedback
level.

### env((gate, signal), a=0.01, r=10, acur=0.5, rcur=0.5)

`env(signal)` is a simple attack-release signal envelope. Attack and release
time are specified in seconds. The curve of the attack and release parts can be
controlled using `acur` and `rcur` parameters. Values are in the range of
[0..1] and make logarithm or exponential curves. The value of 0.5 makes a
linear shape.

> Example: [env(sin(seq(240,440,480)),0.01,0.3,0.5,0.2)](http://localhost:8000/#env(sin(seq(240%2C440%2C480))%2C0.01%2C0.3%2C0.5%2C0.2))

### mix(signal...)

`mix(z1, z2, ...)` mixes signals together. It soft-clips the resulting signal if the
value is outside of the [-1..1] range.

> Example: [mix(0.3*sin(440),0.7*tri(220))](http://localhost:8000/#mix(0.3*sin(440)%2C0.7*tri(220)))

## Notes and melody

### C0..B#8

Notes in Glitch are represented as numbers, often integer. The note of A4 is
considered to be 0. A#4 = 1, G#4 = -1 etc.

Glitch provides a set of useful constants for note indices (not frequencies).
Any note in the range C0..B#8 can be represented as a constant.

### hz(note)

`hz(note)` returns note frequency in hertz.

> Example: [sin(hz(A4))](http://localhost:8000/#sin(hz(A4))) - plays sine wave at 440 Hz, which is A4 note.

### scale(index, mode=0)

`scale(i, mode)` returns a note index at position i in the given scale. Default
mode is major scale.

> Example: [tri(hz(scale(seq(480,r(5)))))](http://localhost:8000/#tri(hz(scale(seq(480%2Cr(5)))))) - plays random notes from the major scale

## Polyphony and live MIDI input

### each(vars, expr, values...)

`each(vars, expr, values...)` applies and adds up the given expression
evaluated for each of the values from the list.  Useful to construct chords.

> Example: [each((vol,note),vol*sin(hz(note)),(0.8,0),(0.5,4),(0.5,7))](http://localhost:8000/#each((vol%2Cnote)%2Cvol*sin(hz(note))%2C(0.8%2C0)%2C(0.5%2C4)%2C(0.5%2C7))%0A)

### MIDI input

If a MIDI keyboard is available, the following variables are set as you press the keys:

* `k0, k1, k2, ..., k9` are MIDI keyboard note indices that are currently played.
* `v0, v1, v2, ..., v9` are MIDI keyboard note velocities in the range [0..1]. Velocity quickly fades out if the key is released.
* `g0, g1, g2, ..., g9` are MIDI keyboard gate values that are either 1 if key is pressed or NaN otherwise.

Additionally, there are two variables, `x` and `y`, that are controlled using
the pitch and the mod wheel of the MIDI keyboard. They can also be controlled
my moving a mouse custor while keeping the Control key pressed.

### bpm

`bpm` is a special variable that allows to specify a tempo of the song. It is
used to synchronize user input with the song tempo, so that the changes user
makes in the formula would not interrupt the rhythm of the song.

> Example: `bpm=120/4` - set tempo to 4 beats at 120 beat per minute.

## Macros

To make reusable snippets of code and to simplify dealing with polyphony Glitch
supports macros. Macros look like functions and are automatically expended as you use them.

Macro definition starts with a dollar sign function with the macro name as the
first parameter: `$(mymacro, ...)`. Macro can be later be called by its name,
e.g. `mymacro()`.

All the parameters passed into the macro appear as `$1`, `$2`, ... variables.

The body of the macro is wrapped into extra parenthesis so you can put multiple
expressions into the macro.

[short-circuit]:
[operator-priority]: 
[bytebeat]:
