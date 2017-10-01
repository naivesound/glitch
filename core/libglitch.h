#ifndef LIBGLITCH_H
#define LIBGLITCH_H

#include <assert.h>
#include <math.h>
#include <stdlib.h>

#ifdef LIBGLITCH_TEST
//
// Test helpers: assert and benchmark macros
//
#include <stdio.h>
#include <time.h>
#define libglitch_assert(cond) (!(cond) ? printf("FAIL: %s\n", #cond) : 0);
#define libglitch_bench_clock() ((float)clock() * 1000000000L / CLOCKS_PER_SEC)
#define libglitch_bench(_name, _iter)                                          \
  for (long _start_ns = libglitch_bench_clock(), _i = (_iter);                 \
       _i > 0 || (printf("BENCH: %30s: %f ns/op\n", _name,                     \
			 (libglitch_bench_clock() - _start_ns) / _iter),       \
		  0);                                                          \
       _i--)
// Number of benchmark iterations
static long N = 1000000L;
// Variables used inside the benchmark loop to prevent loop optimizations
static volatile float x, y;

#endif /* LIBGLITCH_TEST */

// Audio engine sample rate. Used to advance oscillators at the given frequency.
static int libglitch_sample_rate;

// Interpolation util. XXX currently does not interploate at all.
static inline float libglitch_interpolate(float *arr, size_t len, float index) {
  (void)len;
  if (isnan(index)) {
    return NAN;
  }
  return arr[(int)index];
}

// Returns fractal part of the real number TODO rename to libglitch_frac
static float libglitch_wrap(float x) {
#if 0
  return x - floorf(x);
#elif 0
  return fmodf(1 + fmodf(x, 1), 1);
#else
  x = x - (int)x + 1;
  return x - (int)x;
#endif
}

// =================================
// r: pseudo-random number generator
// =================================
static unsigned long long libglitch_rand_seed;

static inline void libglitch_rand_init(unsigned long long seed) {
  libglitch_rand_seed = seed;
}

static inline float libglitch_rand(float max) {
  libglitch_rand_seed = 6364136223846793005ULL * libglitch_rand_seed + 1;
  return (libglitch_rand_seed >> 33) * max / 0x7fffffff;
}

#ifdef LIBGLITCH_TEST
static inline void libglitch_rand_test() {
  // Two random numbers are unlikely to be equal
  libglitch_assert(libglitch_rand(10000) != libglitch_rand(10000));
  // Random with zero range should return zero
  libglitch_assert(libglitch_rand(0) == 0);
  // If any of the parameters is NAN - libglitch_rand() returns NAN
  libglitch_assert(isnan(libglitch_rand(NAN)));
  // Random numbers should always be within the [min, max] range
  for (int i = 0; i < 100000; i++) {
    libglitch_assert(libglitch_rand(1) >= 0 && libglitch_rand(1) <= 1);
  }

  libglitch_bench("rand()", N) { y = libglitch_rand(x); }
}
#endif

// ==============================
// byte: bytebeat range converter
// ==============================
static float libglitch_byte_lut[256];

static inline void libglitch_byte_init() {
  for (int i = 0; i < 256; i++) {
    libglitch_byte_lut[i] = (i - 127) / 128.f;
  }
}
static inline float libglitch_byte(float byte) {
  return libglitch_byte_lut[((int)byte) & 255] + (byte - byte);
}

#ifdef LIBGLITCH_TEST
static void libglitch_byte_test() {
  // Should convert values from [0..255] to the [-1..1] range */
  libglitch_assert(libglitch_byte(255) == 1);
  libglitch_assert(libglitch_byte(255.1f) == 1);
  libglitch_assert(libglitch_byte(255 + 256) == 1);
  libglitch_assert(libglitch_byte(255 - 256) == 1);
  libglitch_assert(libglitch_byte(127) == 0);
  libglitch_assert(libglitch_byte(127 + 256) == 0);
  libglitch_assert(libglitch_byte(127 + 1024) == 0);
  libglitch_assert(libglitch_byte(127 - 256) == 0);
  libglitch_assert(libglitch_byte(-129) == 0);

  libglitch_assert(isnan(libglitch_byte(NAN)));

  libglitch_bench("byte()", N) { y = libglitch_byte(x); }
}
#endif

// =====================================
// hz: note index to frequency converter
// =====================================
#define libglitch_hz_lut_min_note -48 // ~27Hz
#define libglitch_hz_lut_max_note 72  // ~28kHz
#define libglitch_hz_lut_microtones 12
#define libglitch_hz_lut_len                                                   \
  ((libglitch_hz_lut_max_note - libglitch_hz_lut_min_note) *                   \
   libglitch_hz_lut_microtones)
static float libglitch_hz_lut[libglitch_hz_lut_len];

static inline void libglitch_hz_init() {
  for (int n = libglitch_hz_lut_min_note; n < libglitch_hz_lut_max_note; n++) {
    for (int m = 0; m < libglitch_hz_lut_microtones; m++) {
      float note = n + 1.f * m / libglitch_hz_lut_microtones;
      float freq = powf(2.0, note / 12.f) * 440.f;
      int index =
	  (n - libglitch_hz_lut_min_note) * libglitch_hz_lut_microtones + m;
      libglitch_hz_lut[index] = freq;
    }
  }
}

static inline float libglitch_hz(float note) {
  float mul = 1.f;
  while (note < libglitch_hz_lut_min_note) {
    note = note + 12.f;
    mul = mul * 2.f;
  }
  while (note >= libglitch_hz_lut_max_note) {
    note = note - 12.f;
    mul = mul / 2.f;
  }
  return mul * libglitch_interpolate(libglitch_hz_lut, libglitch_hz_lut_len,
				     libglitch_hz_lut_microtones *
					 (note - libglitch_hz_lut_min_note));
}

#ifdef LIBGLITCH_TEST
static void libglitch_hz_test() {
  libglitch_assert(libglitch_hz(0) == 440.f);
  libglitch_assert(libglitch_hz(12) == 880.f);
  libglitch_assert(libglitch_hz(-12) == 220.f);
  libglitch_assert(libglitch_hz(-24) == 110.f);
  libglitch_assert(libglitch_hz(3.2) > libglitch_hz(3));
  libglitch_assert(libglitch_hz(3.2) < libglitch_hz(4));
  libglitch_assert(isnan(libglitch_hz(NAN)));

  libglitch_bench("hz()", N) { y = libglitch_hz(x); }
}
#endif

// =====================================
// sin, tri, saw, sqr: basic oscillators
// =====================================
#define LIBGLITCH_OSC_LUT_LEN 8192
static float libglitch_sin_lut[LIBGLITCH_OSC_LUT_LEN];
static float libglitch_tri_lut[LIBGLITCH_OSC_LUT_LEN];
static float libglitch_saw_lut[LIBGLITCH_OSC_LUT_LEN];

typedef struct libglitch_osc { float w; } libglitch_osc_t;

static void libglitch_osc_init() {
  for (unsigned int i = 0; i < LIBGLITCH_OSC_LUT_LEN; i++) {
    float w = i * 1.f / LIBGLITCH_OSC_LUT_LEN;
    float sign = (w < 0 ? -1 : 1);
    float tri_w = libglitch_wrap(w * sign + 0.25f) - 0.5f;
    libglitch_sin_lut[i] = sinf(w * 2 * 3.14151926f);
    libglitch_tri_lut[i] = sign * 4 * (0.25f - tri_w * (tri_w < 0 ? -1 : 1));
    libglitch_saw_lut[i] = 2 * libglitch_wrap(w + 0.5f * sign) - sign;
  }
}

static float libglitch_osc(libglitch_osc_t *osc, const float freq,
			   float *sample, const size_t len) {
  if (isnan(freq)) {
    return NAN;
  }
  float w = osc->w;
  osc->w = libglitch_wrap(osc->w + freq / libglitch_sample_rate);
  return libglitch_interpolate(sample, len, (w * len));
}

static float libglitch_sin(libglitch_osc_t *osc, float freq) {
  return libglitch_osc(osc, freq, libglitch_sin_lut, LIBGLITCH_OSC_LUT_LEN);
}

static float libglitch_tri(libglitch_osc_t *osc, float freq) {
  return libglitch_osc(osc, freq, libglitch_tri_lut, LIBGLITCH_OSC_LUT_LEN);
}

static float libglitch_saw(libglitch_osc_t *osc, float freq) {
  return libglitch_osc(osc, freq, libglitch_saw_lut, LIBGLITCH_OSC_LUT_LEN);
}

static float libglitch_sqr(libglitch_osc_t *osc, float freq, float pwm) {
  if (isnan(freq)) {
    return NAN;
  }
  float w = osc->w;
  osc->w = libglitch_wrap(osc->w + freq / libglitch_sample_rate);
  return (w < pwm ? 1 : -1);
}

#ifdef LIBGLITCH_TEST
static void libglitch_osc_test() {
  libglitch_osc_t osc = {0};
  x = 440;
  libglitch_bench("sin()", N) { y = libglitch_sin(&osc, x); }
  libglitch_bench("tri()", N) { y = libglitch_saw(&osc, x); }
  libglitch_bench("saw()", N) { y = libglitch_tri(&osc, x); }
  libglitch_bench("sqr()", N) { y = libglitch_sqr(&osc, x, 0.5); }
}
#endif

// ==================================
// lpf, hpf, bpf, bsf: biquad filters
// ==================================
typedef enum libglitch_biquad_filter {
  LIBGLITCH_FILTER_LPF,
  LIBGLITCH_FILTER_HPF,
  LIBGLITCH_FILTER_BPF,
  LIBGLITCH_FILTER_BSF,
} libglitch_biquad_filter_t;

typedef struct libglitch_biquad {
  float x1;
  float x2;
  float y1;
  float y2;
} libglitch_biquad_t;

static inline float libglitch_biquad(libglitch_biquad_t *filter,
				     libglitch_biquad_filter_t type,
				     float input, float cutoff, float q) {
  if (isnan(input) || isnan(cutoff) || isnan(q)) {
    filter->x1 = filter->x2 = filter->y1 = filter->y2 = 0;
    return NAN;
  }
  if (cutoff <= 0 || q <= 0) {
    return 0;
  }

  float w0 = cutoff / libglitch_sample_rate;
  float cs =
      libglitch_interpolate(libglitch_sin_lut, LIBGLITCH_OSC_LUT_LEN,
			    LIBGLITCH_OSC_LUT_LEN * libglitch_wrap(w0 + 0.25f));
  float sn = libglitch_interpolate(libglitch_sin_lut, LIBGLITCH_OSC_LUT_LEN,
				   LIBGLITCH_OSC_LUT_LEN * libglitch_wrap(w0));
  float alpha = sn / (2 * q);
  float a0, a1, a2, b0, b1, b2;

  switch (type) {
  case LIBGLITCH_FILTER_LPF:
    b0 = (1 - cs) / 2;
    b1 = 1 - cs;
    b2 = (1 - cs) / 2;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
    break;
  case LIBGLITCH_FILTER_HPF:
    b0 = (1 + cs) / 2;
    b1 = -(1 + cs);
    b2 = (1 + cs) / 2;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
    break;
  case LIBGLITCH_FILTER_BPF:
    b0 = alpha;
    b1 = 0;
    b2 = -alpha;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
    break;
  case LIBGLITCH_FILTER_BSF:
    b0 = 1;
    b1 = -2 * cs;
    b2 = 1;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
    break;
  default:
    return input;
  }

  float out = (b0 / a0) * input + b1 / a0 * filter->x1 + b2 / a0 * filter->x2 -
	      a1 / a0 * filter->y1 - a2 / a0 * filter->y2;

  filter->x2 = filter->x1;
  filter->x1 = input;
  filter->y2 = filter->y1;
  filter->y1 = out;
  return out;
}

#ifdef LIBGLITCH_TEST
static void libglitch_biquad_test() {
  libglitch_biquad_t filter = {0.f, 0.f, 0.f, 0.f};
  x = 42;
  libglitch_bench("lpf()", N) {
    x = libglitch_biquad(&filter, LIBGLITCH_FILTER_LPF, x, x, x);
  }
}
#endif

// ============================
// env: attack-release envelope
// ============================
typedef struct libglitch_env {
  int t;
  int at;
  int rt;
  float ac;
  float aval;
  float amul;
  float adif;
  float rc;
  float rval;
  float rmul;
  float rdif;
} libglitch_env_t;
#define libglitch_env_calc_exp(t, c, mult, diff)                               \
  do {                                                                         \
    if ((t) > 0) {                                                             \
      if ((c) < 0.4999f || (c) > 0.5001f) {                                    \
	float s = (1 - (c)) / (c);                                             \
	(mult) = powf(s, 2.0f / (t));                                          \
	(diff) = ((mult)-1) / (s * s - 1);                                     \
      } else {                                                                 \
	(mult) = 1;                                                            \
	(diff) = 1.f / (t);                                                    \
      }                                                                        \
    }                                                                          \
  } while (0)

static inline float libglitch_env_flim(float x, float a, float b) {
  if (!isnan(a) && x < a) {
    return a;
  } else if (!isnan(b) && x > b) {
    return b;
  }
  return x;
}
static float libglitch_env(libglitch_env_t *env, float g, float v, float at,
			   float rt, float ac, float rc) {
  if (env->t == 0) {
    env->at = (int)(at * libglitch_sample_rate);
    env->rt = (int)(rt * libglitch_sample_rate);
    ac = libglitch_env_flim(ac, 0.0001f, 0.9999f);
    rc = libglitch_env_flim(rc, 0.0001f, 0.9999f);

    libglitch_env_calc_exp(env->at, ac, env->amul, env->adif);
    libglitch_env_calc_exp(env->rt, (1 - rc), env->rmul, env->rdif);
    env->aval = env->rval = 0;
  }

  float r = 0;
  if (env->t < env->at) {
    r = env->aval = env->aval * env->amul + env->adif;
  } else if (env->t < env->at + env->rt) {
    env->rval = env->rval * env->rmul + env->rdif;
    r = 1 - env->rval;
  }

  env->t++;
  if (isnan(g)) {
    env->t = 0;
    r = NAN;
  }
  return r * v;
}
#ifdef LIBGLITCH_TEST
static void libglitch_env_test() {
  libglitch_env_t env = {0};
  x = 42;
  libglitch_bench("env()", N) {
    x = libglitch_env(&env, x, x, 0.01, 0.5, 0.2, 0.6);
  }
}
#endif

// ======================================
// delay: simple delay line with feedback
// ======================================
#define LIBGLITCH_MAX_DELAY_TIME 10    /* seconds */
#define LIBGLITCH_MIN_DELAY_BLOCK 8192 /* smallest delay buffer resize */
typedef struct libglitch_delay {
  float *buf;
  size_t n;
  int pos;
} libglitch_delay_t;

static float libglitch_delay(libglitch_delay_t *delay, float signal, float time,
			     float level, float feedback) {
  if (feedback > 1.0f) {
    feedback = 1.0f;
  }
  if (feedback < 0.0f) {
    feedback = 0.0f;
  }
  if (time <= 0) {
    return signal;
  }
  if (time > LIBGLITCH_MAX_DELAY_TIME) {
    time = LIBGLITCH_MAX_DELAY_TIME;
  }

  size_t bufsz = (size_t)(time * libglitch_sample_rate);

  /* Expand buffer if needed */
  if (delay->n < bufsz) {
    int sz =
	((bufsz / LIBGLITCH_MIN_DELAY_BLOCK) + 1) * LIBGLITCH_MIN_DELAY_BLOCK;
    int n = delay->n;
    delay->n = sz;
    delay->buf = realloc(delay->buf, sz * sizeof(*delay->buf));
    if (delay->buf == NULL) {
      delay->n = 0;
      return signal;
    }
    for (int i = n; i < sz; i++) {
      delay->buf[i] = 0;
    }
  }

  /* Get value delayed value from the buffer */
  float out = 0;
  if (bufsz <= delay->n) {
    unsigned int i = (delay->pos + delay->n - bufsz) % delay->n;
    out = delay->buf[i] * level;
  }

  /* Write updated value to the buffer */
  signal = (isnan(signal) ? 0 : signal);
  delay->buf[delay->pos] = delay->buf[delay->pos] * feedback + signal;
  delay->pos = (delay->pos + 1) % delay->n;
  return signal + out;
}

static void libglitch_delay_free(libglitch_delay_t *delay) { free(delay->buf); }

#ifdef LIBGLITCH_TEST
static void libglitch_delay_test() {
  libglitch_delay_t delay = {0};
  x = 42;
  libglitch_bench("delay()", N) {
    x = libglitch_delay(&delay, x, 0.25, 0.5, 0.2);
  }
  libglitch_delay_free(&delay);
}
#endif

// ===============================
// pluck: Karplus-Strong algorithm
// ===============================
typedef struct libglitch_pluck {
  int init; /* FIXME: can we use sample != NULL instead? */
  int t;
  float *sample;
} libglitch_pluck_t;

static float libglitch_pluck(libglitch_pluck_t *pluck, float freq, float decay,
			     float (*fill)(void *context), void *context) {

  if (isnan(freq)) {
    pluck->init = 0;
    return NAN;
  } else if (freq == 0) {
    return 0;
  } else if (freq < 0) {
    freq = -freq;
  }

  int n = (int)(libglitch_sample_rate / freq);
  if (n == 0) {
    return 0;
  }

  if (pluck->init == 0) {
    if (pluck->sample == NULL) {
      pluck->sample = (float *)malloc(sizeof(float) * n);
    }
    for (int i = 0; i < n; i++) {
      if (fill != NULL) {
	pluck->sample[i] = fill(context);
      } else {
	pluck->sample[i] = (rand() * 2.0f / RAND_MAX) - 1.0f;
      }
    }
    pluck->init = 1;
    pluck->t = 0;
  }
  float x = pluck->sample[pluck->t % n];
  float y = pluck->sample[(pluck->t + 1) % n];
  pluck->t = (pluck->t + 1) % n;
  pluck->sample[pluck->t] = x * decay + y * (1 - decay);
  return x;
}

static void libglitch_pluck_free(libglitch_pluck_t *pluck) {
  free(pluck->sample);
}

#ifdef LIBGLITCH_TEST
static void libglitch_pluck_test() {
  libglitch_pluck_t pluck = {0};
  x = 42;
  libglitch_bench("pluck()", N) {
    x = libglitch_pluck(&pluck, 0.25, 0.5, NULL, NULL);
  }
  libglitch_pluck_free(&pluck);
}
#endif

// ========================
// libglitch initialization
// ========================
static void libglitch_init(int sample_rate, unsigned long long seed) {
  libglitch_sample_rate = sample_rate;

  libglitch_rand_init(seed);
  libglitch_byte_init();
  libglitch_hz_init();
  libglitch_osc_init();
}

#ifdef LIBGLITCH_TEST
static void libglitch_test() {
  libglitch_rand_test();
  libglitch_byte_test();
  libglitch_hz_test();
  libglitch_osc_test();
  libglitch_biquad_test();
  libglitch_env_test();
  libglitch_delay_test();
  libglitch_pluck_test();
}
#endif /* LIBGLITCH_TEST */

#endif /* LIBGLITCH_H */
