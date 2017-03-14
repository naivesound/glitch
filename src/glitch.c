#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "glitch.h"
#include "piano.h"
#include "tr808.h"

#include "expr.h"

static int SAMPLE_RATE = 48000;

static glitch_loader_fn loader = NULL;

#define PI 3.1415926f
#define MIN(a, b) ((a) < (b) ? (a) : (b))

static float arg(vec_expr_t args, int n, float defval) {
  if (vec_len(&args) < n + 1) {
    return defval;
  }
  return expr_eval(&vec_nth(&args, n));
}

struct osc_context {
  float freq;
  float w;
};

struct fm_context {
  float freq;
  int sync;
  float prev;

  float w0;
  float w1;
  float w2;
  float w3;
};

typedef vec(float) vec_float_t;

struct pluck_context {
  int init;
  int t;
  float *sample;
};

struct seq_context {
  int init;
  int hasnan;
  float mul;
  int beat;
  int t;

  float value;
  vec_float_t values;
};

struct env_context {
  int init;
  vec_float_t d;
  vec_float_t e;
  int t;
  int segment;
};

struct mix_context {
  int init;
  vec_float_t values;
};

struct filter_context {
  float x1;
  float x2;
  float y1;
  float y2;
};

struct delay_context {
  float *buf;
  unsigned int pos;
  size_t size;
  float prev;
};

struct each_context {
  int init;
  vec_expr_t args;
};

struct sample_context {
  float t;
};

static float lib_byte(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  float x = arg(args, 0, 127);
  if (isnan(x)) {
    return NAN;
  }
  return (((int)x & 255) - 127) / 128.0;
}

static float lib_s(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  return sinf(arg(args, 0, 0) * PI / 180.0f);
}

static float lib_r(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  return rand() * arg(args, 0, 1) / RAND_MAX;
}

static float lib_l(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  if (arg(args, 0, 0)) {
    return log2f(arg(args, 0, 0));
  }
  return 0;
}

static float lib_a(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  float index = arg(args, 0, NAN);
  if (isnan(index)) {
    return NAN;
  }
  int len = vec_len(&args) - 1;
  if (len == 0) {
    return 0;
  }
  int i = (int)floor(index + len) % len;
  i = (i + len) % len;
  return arg(args, i + 1, 0);
}

static int scales[][13] = {
    {7, 0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0},      // ionian, major scale
    {7, 0, 2, 3, 5, 7, 9, 10, 0, 0, 0, 0},      // dorian
    {7, 0, 1, 3, 5, 7, 8, 10, 0, 0, 0, 0},      // phrygian
    {7, 0, 2, 4, 6, 7, 9, 11, 0, 0, 0, 0},      // lydian
    {7, 0, 2, 4, 5, 7, 9, 10, 0, 0, 0, 0},      // mixolydian
    {7, 0, 2, 3, 5, 7, 8, 10, 0, 0, 0, 0},      // aeolian, natural minor scale
    {7, 0, 1, 3, 5, 6, 8, 10, 0, 0, 0, 0},      // locrian
    {7, 0, 2, 3, 5, 7, 8, 11, 0, 0, 0, 0},      // harmonic minor scale
    {7, 0, 2, 3, 5, 7, 9, 11, 0, 0, 0, 0},      // melodic minor scale
    {5, 0, 2, 4, 7, 9, 0, 0, 0, 0, 0, 0},       // major pentatonic
    {5, 0, 3, 5, 7, 10, 0, 0, 0, 0, 0, 0},      // minor pentatonic
    {6, 0, 3, 5, 6, 7, 10, 0, 0, 0, 0},         // blues
    {6, 0, 2, 4, 6, 8, 10, 0, 0, 0, 0},         // whole tone scale
    {8, 0, 1, 3, 4, 6, 7, 9, 10, 0, 0, 0},      // octatonic
    {12, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, // chromatic is a fallback scale
};

static float lib_scale(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  float note = arg(args, 0, 0);
  float scale = arg(args, 1, 0);
  if (isnan(scale) || isnan(note)) {
    return NAN;
  }
  if (scale < 0 || scale > 14) {
    scale = 14;
  }
  int len = scales[(int)scale][0];
  int n = (int)note;
  int transpose = n / len * 12;
  n = (n + len) % len;
  n = (n + len) % len;
  return scales[(int)scale][n + 1] + transpose;
}

static float lib_hz(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  (void)context;
  return powf(2, arg(args, 0, 0) / 12) * 440;
}

static float lib_each(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct each_context *each = (struct each_context *)context;
  float r = NAN;

  if (vec_len(&args) < 3) {
    return NAN;
  }

  if (!each->init) {
    each->init = 1;
    for (int i = 0; i < vec_len(&args) - 2; i++) {
      struct expr tmp = {0};
      expr_copy(&tmp, &vec_nth(&args, 1));
      vec_push(&each->args, tmp);
    }
  }

  // List of variables
  struct expr *init = &vec_nth(&args, 0);
  float mix = 0.0f;
  for (int i = 0; i < vec_len(&each->args); i++) {
    struct expr *ilist = init;
    struct expr *alist = &vec_nth(&args, i + 2);
    struct expr *acar = NULL;
    struct expr *icar = NULL;
    while (ilist->type == OP_COMMA) {
      icar = &vec_nth(&ilist->param.op.args, 0);
      ilist = &vec_nth(&ilist->param.op.args, 1);
      acar = alist;
      if (alist->type == OP_COMMA) {
        acar = &vec_nth(&alist->param.op.args, 0);
        alist = &vec_nth(&alist->param.op.args, 1);
      }
      if (icar->type == OP_VAR) {
        *icar->param.var.value = expr_eval(acar);
      }
    }
    if (ilist->type == OP_VAR) {
      *ilist->param.var.value = expr_eval(alist);
    }
    r = expr_eval(&vec_nth(&each->args, i));
    if (!isnan(r)) {
      mix = mix + r;
    }
  }
  return mix / sqrt(vec_len(&each->args));
}

static void lib_each_cleanup(struct expr_func *f, void *context) {
  (void)f;
  int i;
  struct expr e;
  struct each_context *each = (struct each_context *)context;
  vec_foreach(&each->args, e, i) { expr_destroy_args(&e); }
  vec_free(&each->args);
}

static inline float fwrap(float x) {
  float i;
  return modff(x, &i);
}
static inline float fsign(float x) { return (x < 0 ? -1 : 1); }

static float lib_osc(struct expr_func *f, vec_expr_t args, void *context) {
  struct osc_context *osc = (struct osc_context *)context;
  float freq = arg(args, 0, NAN);
  osc->w = fwrap(osc->w + osc->freq / SAMPLE_RATE);
  if (isnan(freq)) {
    return NAN;
  }
  osc->freq = freq;
  float w = osc->w;
  if (strncmp(f->name, "sin", 4) == 0) {
    return sinf(w * 2 * PI);
  } else if (strncmp(f->name, "tri", 4) == 0) {
    w = fwrap(w + 0.25f * fsign(w)) - 0.5 * fsign(w);
    return 4 * w * fsign(w) - 1;
  } else if (strncmp(f->name, "saw", 4) == 0) {
    return 2 * w - fsign(w);
  } else if (strncmp(f->name, "sqr", 4) == 0) {
    w = w - floor(w);
    return w < arg(args, 1, 0.5) ? 1 : -1;
  }
  return 0;
}

static float lib_fm(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct fm_context *fm = (struct fm_context *)context;
  float freq = arg(args, 0, NAN);
  float mf1 = arg(args, 1, 0);
  float mi1 = arg(args, 2, 0);
  float mf2 = arg(args, 3, 0);
  float mi2 = arg(args, 4, 0);
  float mf3 = arg(args, 5, 0);
  float mi3 = arg(args, 6, 0);

  fm->w3 = fwrap(fm->w3 + mf3 * fm->freq / SAMPLE_RATE);
  fm->w2 = fwrap(fm->w2 + mf2 * fm->freq / SAMPLE_RATE);
  fm->w1 = fwrap(fm->w1 + mf1 * fm->freq / SAMPLE_RATE);
  fm->w0 = fwrap(fm->w0 + fm->freq / SAMPLE_RATE);

  float v3 = mi3 * sin(2 * PI * fm->w3);
  float v2 = mi2 * sin(2 * PI * fm->w2);
  float v1 = mi1 * sin(2 * PI * (fm->w1 + v3));
  float v0 = sin(2 * PI * (fm->w0 + v1 + v2));

  if (isnan(freq)) {
    fm->sync = 1;
    return NAN;
  } else {
    fm->freq = freq;
  }

  if (fm->sync && v0 >= 0 && fm->prev <= 0) {
    fm->sync = 0;
    fm->w0 = fm->w1 = fm->w2 = fm->w3 = 0;
  }

  fm->prev = v0;

  return v0;
}

static float lib_seq(struct expr_func *f, vec_expr_t args, void *context) {
  struct seq_context *seq = (struct seq_context *)context;

  if (!seq->init) {
    seq->mul = 1;
    seq->init = 1;
    if (vec_len(&args) > 0) {
      struct expr *bpm = &vec_nth(&args, 0);
      if (bpm->type == OP_COMMA) {
        float beat = expr_eval(&vec_nth(&bpm->param.op.args, 0));
        float tempo = expr_eval(bpm);
        seq->beat = to_int(beat);
        seq->t = (int)((beat - floorf(beat)) * SAMPLE_RATE / (tempo / 60.0));
      }
    }
  }
  float beatlen = SAMPLE_RATE / (arg(args, 0, NAN) / 60) * seq->mul;
  if (isnan(beatlen)) {
    return NAN;
  }
  int t = seq->t;
  seq->t = seq->t + 1;
  int len = vec_len(&args) - 1;
  if (len <= 0) {
    return (t == 0 ? NAN : 0);
  }
  int i = seq->beat % len;
  if (seq->t >= beatlen) {
    seq->t = 0;
    seq->beat++;
  }

  struct expr *e = &vec_nth(&args, i + 1);
  if (strncmp(f->name, "seq", 4) == 0) {
    if (t == 0 || seq->hasnan) {
      seq->hasnan = 0;
      float v;
      vec_free(&seq->values);
      if (e->type == OP_COMMA) {
        v = expr_eval(&vec_nth(&e->param.op.args, 0));
        if (isnan(v)) {
          seq->hasnan = 1;
        }
        seq->mul = v;
        e = &vec_nth(&e->param.op.args, 1);
        if (e->type == OP_COMMA) {
          while (e->type == OP_COMMA) {
            float v = expr_eval(&vec_nth(&e->param.op.args, 0));
            if (isnan(v)) {
              seq->hasnan = 1;
            }
            vec_push(&seq->values, v);
            e = &vec_nth(&e->param.op.args, 1);
          }
          v = expr_eval(e);
          if (isnan(v)) {
            seq->hasnan = 1;
          }
          vec_push(&seq->values, v);
          return NAN;
        }
      } else {
        seq->mul = 1;
      }
      v = expr_eval(e);
      if (isnan(v)) {
        seq->hasnan = 1;
      }
      seq->value = v;
      return NAN;
    }

    len = vec_len(&seq->values);
    if (len == 0) {
      return seq->value;
    } else {
      int n = len - 1;
      int i = (int)(len * t / beatlen);
      float k = (t / beatlen - i / n) * n;
      float from = vec_nth(&seq->values, i);
      float to = from;
      if (len > i) {
        to = vec_nth(&seq->values, i + 1);
      }
      return from + (to - from) * k;
    }
  } else if (strncmp(f->name, "loop", 5) == 0) {
    if (e->type == OP_COMMA) {
      seq->mul = expr_eval(&vec_nth(&e->param.op.args, 0));
      e = &vec_nth(&e->param.op.args, 1);
    } else {
      seq->mul = 1;
    }
    float value = expr_eval(e);
    return t == 0 ? NAN : value;
  }
  return 0;
}

static void lib_seq_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct seq_context *seq = (struct seq_context *)context;
  vec_free(&seq->values);
}

static float lib_env(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct env_context *env = (struct env_context *)context;

  // First argument is signal value
  float v = arg(args, 0, NAN);

  // Default time interval between envelope sections is 0.05
  float dt = 0.05;

  if (!env->init) {
    for (int i = 0; i < vec_len(&args); i++) {
      vec_push(&env->d, 0);
      vec_push(&env->e, 0);
    }
    env->init = 1;
  }

  int i, j;
  float level = 0;
  for (i = 1, j = 0; i < vec_len(&args); i++, j++) {
    struct expr *e = &vec_nth(&args, i);
    if (e->type == OP_COMMA) {
      dt = expr_eval(&vec_nth(&e->param.op.args, 0));
      e = &vec_nth(&e->param.op.args, 1);
    }
    level = expr_eval(e);
    if (j == 0 && level < 1) {
      vec_nth(&env->d, 0) = 0;
      vec_nth(&env->e, 0) = 1;
      j++;
    }
    vec_nth(&env->d, j) = dt * SAMPLE_RATE;
    vec_nth(&env->e, j) = level;
  }
  if (level > 0) {
    vec_nth(&env->d, j) = dt * SAMPLE_RATE;
    vec_nth(&env->e, j) = 0;
  }

  if (isnan(v)) {
    env->segment = 0;
    env->t = 0;
    return NAN;
  }
  env->t++;
  if (env->segment < vec_len(&env->d)) {
    if (env->t > vec_nth(&env->d, env->segment)) {
      env->t = 0;
      env->segment++;
    }
  } else {
    return 0; // end of envelope
  }
  float prevAmp = (env->segment == 0 ? 0 : vec_nth(&env->e, env->segment - 1));
  float amp = vec_nth(&env->e, env->segment);
  return (v) * (prevAmp +
                (amp - prevAmp) * (env->t / vec_nth(&env->d, env->segment)));
}

static void lib_env_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct env_context *env = (struct env_context *)context;
  vec_free(&env->d);
  vec_free(&env->e);
}

static float lib_mix(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct mix_context *mix = (struct mix_context *)context;
  if (!mix->init) {
    for (int i = 0; i < vec_len(&args); i++) {
      vec_push(&mix->values, 0);
    }
    mix->init = 1;
  }
  float v = 0;
  for (int i = 0; i < vec_len(&args); i++) {
    struct expr *e = &vec_nth(&args, i);
    float vol = 1;
    if (e->type == OP_COMMA) {
      vol = expr_eval(&vec_nth(&e->param.op.args, 0));
      e = &vec_nth(&e->param.op.args, 1);
    }
    float sample = expr_eval(e);
    if (isnan(sample)) {
      sample = vec_nth(&mix->values, i);
    }
    vec_nth(&mix->values, i) = sample;
    v = v + vol * sample;
  }
  if (vec_len(&args) > 0) {
    v = v / sqrtf(vec_len(&args));
    v = ((v < -1) ? -1 : v);
    v = ((v > 1) ? 1 : v);
    return v;
  }
  return 0;
}

static void lib_mix_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct mix_context *mix = (struct mix_context *)context;
  vec_free(&mix->values);
}

static float lib_filter(struct expr_func *f, vec_expr_t args, void *context) {
  struct filter_context *filter = (struct filter_context *)context;
  float signal = arg(args, 0, NAN);
  float cutoff = arg(args, 1, 200);
  float q = arg(args, 2, 1);
  if (isnan(signal) || isnan(cutoff) || isnan(q)) {
    filter->x1 = filter->x2 = filter->y1 = filter->y2 = 0;
    return NAN;
  }
  float w0 = 2 * PI * cutoff / SAMPLE_RATE;
  float cs = cos(w0);
  float sn = sin(w0);
  float alpha = sn / (2 * q);
  float a0, a1, a2, b0, b1, b2;

  if (strncmp(f->name, "lpf", 4) == 0) {
    b0 = (1 - cs) / 2;
    b1 = 1 - cs;
    b2 = (1 - cs) / 2;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
  } else if (strncmp(f->name, "hpf", 4) == 0) {
    b0 = (1 + cs) / 2;
    b1 = -(1 + cs);
    b2 = (1 + cs) / 2;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
  } else if (strncmp(f->name, "bpf", 4) == 0) {
    b0 = alpha;
    b1 = 0;
    b2 = -alpha;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
  } else if (strncmp(f->name, "bsf", 4) == 0) {
    b0 = 1;
    b1 = -2 * cs;
    b2 = 1;
    a0 = 1 + alpha;
    a1 = -2 * cs;
    a2 = 1 - alpha;
  } else {
    return signal;
  }

  float out = (b0 / a0) * signal + b1 / a0 * filter->x1 + b2 / a0 * filter->x2 -
              a1 / a0 * filter->y1 - a2 / a0 * filter->y2;

  filter->x2 = filter->x1;
  filter->x1 = signal;
  filter->y2 = filter->y1;
  filter->y1 = out;
  return out;
}

static float lib_delay(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct delay_context *delay = (struct delay_context *)context;

  float signal = arg(args, 0, NAN);
  float time = arg(args, 1, 0);
  float level = arg(args, 2, 0);
  float feedback = arg(args, 3, 0);
  if (feedback > 1.0f)
    feedback = 1.0f;
  if (feedback < 0.0f)
    feedback = 0.0f;

  size_t bufsz = time * SAMPLE_RATE;

  if (bufsz != delay->size) {
    delay->buf = (float *)realloc(delay->buf, bufsz * sizeof(float));
    memset(delay->buf, 0, bufsz * sizeof(float));
    delay->size = bufsz;
  }
  if (delay->buf == NULL) {
    return signal;
  }
  float out = 0;
  int pos = delay->pos;
  if (isnan(signal)) {
    signal = 0;
  }
  out = delay->buf[pos] * level;
  delay->buf[pos] = delay->buf[pos] * feedback + signal;
  delay->pos = (delay->pos + 1) % delay->size;
  return signal + out;
}

static void lib_delay_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct delay_context *delay = (struct delay_context *)context;
  free(delay->buf);
}

static float int16_sample(unsigned char hi, unsigned char lo) {
  int sign = hi & (1 << 7);
  int v = (((int)(hi)&0x7f) << 8) | (int)lo;
  if (sign) {
    v = -0x8000 + v;
  }
  return v * 1.f / 0x8000;
}

static float lib_tr808(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct sample_context *sample = (struct sample_context *)context;

  float drum = arg(args, 0, NAN);
  float vol = arg(args, 1, 1);
  float shift = arg(args, 2, 0);

  if (isnan(drum) || isnan(vol) || isnan(shift)) {
    sample->t = 0;
    return NAN;
  }

  static unsigned char *samples[] = {
      tr808_bd_wav, tr808_sn_wav, tr808_mt_wav, tr808_mc_wav, tr808_rs_wav,
      tr808_cl_wav, tr808_cb_wav, tr808_oh_wav, tr808_hh_wav,
  };
  static unsigned int len[] = {
      tr808_bd_wav_len, tr808_sn_wav_len, tr808_mt_wav_len,
      tr808_mc_wav_len, tr808_rs_wav_len, tr808_cl_wav_len,
      tr808_cb_wav_len, tr808_oh_wav_len, tr808_hh_wav_len,
  };
  static int N = sizeof(samples) / sizeof(*samples);

  int index = (((int)drum % N) + N) % N;
  unsigned char *pcm = samples[index];
  if (sample->t * 2 + 0x80 + 1 < len[index]) {
    unsigned char hi = pcm[0x80 + (int)sample->t * 2 + 1];
    unsigned char lo = pcm[0x80 + (int)sample->t * 2];
    float x = int16_sample(hi, lo);
    sample->t = sample->t + powf(2.0, shift / 12.0);
    return x * vol;
  }
  return 0;
}

static float lib_piano(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct sample_context *sample = (struct sample_context *)context;

  float freq = arg(args, 0, NAN);
  if (isnan(freq)) {
    sample->t = 0;
    return NAN;
  }
  if (freq == 0) {
    return 0;
  } else if (freq < 0) {
    freq = -freq;
  }

  static unsigned char *samples[] = {
      samples_piano_pianoc2_wav, samples_piano_pianoc4_wav,
      samples_piano_pianoc6_wav,
  };
  static unsigned int len[] = {
      samples_piano_pianoc2_wav_len, samples_piano_pianoc4_wav_len,
      samples_piano_pianoc6_wav_len,
  };

  /* 0 = C0..C3, 1 = C3..C5, 2 = C5..C8 */
  int index = (freq < 130.f ? 0 : (freq < 523.f ? 1 : 2));
  float note = 12.f * log2f(freq / 440.f);
  float base_freq =
      (freq < 130.f ? 65.41f : (freq < 523.f ? 261.63f : 1046.50f));
  float base_note = 12.f * log2f(base_freq / 440.f);
  float shift = (note - base_note);
  unsigned char *pcm = samples[index];
  if (sample->t * 2 + 0x80 + 1 < len[index]) {
    unsigned char hi = pcm[0x80 + (int)sample->t * 2 + 1];
    unsigned char lo = pcm[0x80 + (int)sample->t * 2];
    float x = int16_sample(hi, lo);
    sample->t = sample->t + powf(2.0, shift / 12.0);
    return x;
  }
  return 0;
}

static float lib_sample(struct expr_func *f, vec_expr_t args, void *context) {
  if (loader == NULL) {
    return NAN;
  }
  struct sample_context *sample = (struct sample_context *)context;
  float variant = arg(args, 0, NAN);
  float vol = arg(args, 1, 1);
  float shift = arg(args, 2, 0);
  if (isnan(variant) || isnan(vol) || isnan(shift)) {
    sample->t = 0;
    return NAN;
  }
  sample->t = sample->t + powf(2.0, shift / 12.0);
  return loader(f->name, (int)variant, (int)(sample->t)) * vol;
}

static float lib_pluck(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct pluck_context *pluck = (struct pluck_context *)context;
  float freq = arg(args, 0, NAN);
  float decay = arg(args, 1, 0.5);

  if (isnan(freq)) {
    pluck->init = 0;
    return NAN;
  } else if (freq == 0) {
    return 0;
  } else if (freq < 0) {
    freq = -freq;
  }

  int n = (int)(SAMPLE_RATE / freq);
  if (n == 0) {
    return 0;
  }

  if (pluck->init == 0) {
    if (pluck->sample == NULL) {
      pluck->sample = (float *)malloc(sizeof(float) * SAMPLE_RATE);
    }
    for (int i = 0; i < n; i++) {
      if (vec_len(&args) >= 3) {
        pluck->sample[i] = expr_eval(&vec_nth(&args, 2));
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

static void lib_pluck_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct pluck_context *pluck = (struct pluck_context *)context;
  free(pluck->sample);
}

#define MAX_FUNCS 1024
static struct expr_func glitch_funcs[MAX_FUNCS + 1] = {
    {"byte", lib_byte, NULL, 0},
    {"s", lib_s, NULL, 0},
    {"r", lib_r, NULL, 0},
    {"l", lib_l, NULL, 0},
    {"a", lib_a, NULL, 0},
    {"scale", lib_scale, NULL, 0},
    {"hz", lib_hz, NULL, 0},

    {"each", lib_each, lib_each_cleanup, sizeof(struct each_context)},

    {"sin", lib_osc, NULL, sizeof(struct osc_context)},
    {"tri", lib_osc, NULL, sizeof(struct osc_context)},
    {"saw", lib_osc, NULL, sizeof(struct osc_context)},
    {"sqr", lib_osc, NULL, sizeof(struct osc_context)},
    {"fm", lib_fm, NULL, sizeof(struct fm_context)},
    {"pluck", lib_pluck, lib_pluck_cleanup, sizeof(struct pluck_context)},
    {"tr808", lib_tr808, NULL, sizeof(struct sample_context)},
    {"piano", lib_piano, NULL, sizeof(struct sample_context)},

    {"loop", lib_seq, lib_seq_cleanup, sizeof(struct seq_context)},
    {"seq", lib_seq, lib_seq_cleanup, sizeof(struct seq_context)},

    {"env", lib_env, lib_env_cleanup, sizeof(struct env_context)},

    {"mix", lib_mix, lib_mix_cleanup, sizeof(struct mix_context)},

    {"lpf", lib_filter, NULL, sizeof(struct filter_context)},
    {"hpf", lib_filter, NULL, sizeof(struct filter_context)},
    {"bpf", lib_filter, NULL, sizeof(struct filter_context)},
    {"bsf", lib_filter, NULL, sizeof(struct filter_context)},

    {"delay", lib_delay, lib_delay_cleanup, sizeof(struct delay_context)},
    {NULL, NULL, 0},
};

struct glitch *glitch_create() {
  struct glitch *g = calloc(1, sizeof(struct glitch));
  return g;
}

void glitch_sample_rate(int rate) { SAMPLE_RATE = rate; }

void glitch_set_loader(glitch_loader_fn fn) { loader = fn; }

int glitch_add_sample_func(const char *name) {
  for (int i = 0; i < MAX_FUNCS; i++) {
    if (glitch_funcs[i].name == NULL) {
      glitch_funcs[i].name = name;
      glitch_funcs[i].f = lib_sample;
      glitch_funcs[i].ctxsz = sizeof(struct sample_context);
      glitch_funcs[i + 1].name = NULL;
      return 0;
    }
  }
  return -1;
}

void glitch_destroy(struct glitch *g) {
  expr_destroy(g->e, &g->vars);
  free(g);
}

void glitch_xy(struct glitch *g, float x, float y) {
  g->x->value = x;
  g->y->value = y;
}

void glitch_midi(struct glitch *g, unsigned char cmd, unsigned char a,
                 unsigned char b) {
  cmd = cmd >> 4;
  if (cmd == 0x9 && b > 0) {
    // Note pressed: insert to the head of the "list"
    for (int i = 0; i < MAX_POLYPHONY; i++) {
      if (isnan(g->k[i]->value)) {
        g->k[i]->value = a - 69;
        g->v[i]->value = b / 128.0;
        break;
      }
    }
  } else if ((cmd == 0x9 && b == 0) || cmd == 0x8) {
    // Note released: remove from the "list" and shift the rest
    float key = a - 69;
    for (int i = 0; i < MAX_POLYPHONY; i++) {
      if (g->k[i]->value == key) {
        g->k[i]->value = g->v[i]->value = NAN;
        break;
      }
    }
  } else if (cmd == 0xe) {
    // Pitch bend wheel
    g->x->value = (b - 64.f) / 65.f;
  } else if (cmd == 0xb && a == 1) {
    // Control change message: mod wheel
    g->y->value = (b - 64.f) / 65.f;
  } else {
    fprintf(stderr, "MIDI command %d %d %d\n", cmd, a, b);
  }
}

int glitch_compile(struct glitch *g, const char *s, size_t len) {
  if (!g->init) {
    g->t = expr_var(&g->vars, "t", 1);
    g->x = expr_var(&g->vars, "x", 1);
    g->y = expr_var(&g->vars, "y", 1);
    g->bpm = expr_var(&g->vars, "bpm", 3);

    for (int i = 0; i < MAX_POLYPHONY; i++) {
      char name[4];
      snprintf(name, sizeof(name), "k%d", i);
      g->k[i] = expr_var(&g->vars, name, strlen(name));
      snprintf(name, sizeof(name), "v%d", i);
      g->v[i] = expr_var(&g->vars, name, strlen(name));
      g->k[i]->value = g->v[i]->value = NAN;
    }

    /* Note constants */
    const struct {
      char *name;
      int pitch;
    } notes[] = {
        {"C0", -9},  {"C#0", -8}, {"Cb0", -10}, {"D0", -7},  {"D#0", -6},
        {"Db0", -8}, {"E0", -5},  {"E#0", -4},  {"Eb0", -6}, {"F0", -4},
        {"F#0", -3}, {"Fb0", -5}, {"G0", -2},   {"G#0", -1}, {"Gb0", -3},
        {"A0", 0},   {"A#0", 1},  {"Ab0", -1},  {"B0", 2},   {"B#0", 3},
        {"Bb0", 1},

    };
    for (int octave = -4; octave < 4; octave++) {
      char buf[4];
      for (unsigned int n = 0; n < sizeof(notes) / sizeof(notes[0]); n++) {
        strncpy(buf, notes[n].name, sizeof(buf));
        buf[strlen(buf) - 1] = '0' + octave + 4;
        int note = notes[n].pitch + octave * 12;
        expr_var(&g->vars, buf, strlen(buf))->value = note;
      }
    }

    /* TR808 drum constants */
    expr_var(&g->vars, "BD", 3)->value = 0;
    expr_var(&g->vars, "SD", 3)->value = 1;
    expr_var(&g->vars, "MT", 3)->value = 2;
    expr_var(&g->vars, "MA", 3)->value = 3;
    expr_var(&g->vars, "RS", 3)->value = 4;
    expr_var(&g->vars, "CP", 3)->value = 5;
    expr_var(&g->vars, "CB", 7)->value = 6;
    expr_var(&g->vars, "OH", 3)->value = 7;
    expr_var(&g->vars, "HH", 3)->value = 8;

    g->init = 1;
  }
  struct expr *e = expr_create(s, len, &g->vars, glitch_funcs);
  if (e == NULL) {
    return -1;
  }
  expr_destroy(g->next_expr, NULL);
  g->next_expr = e;
  return 0;
}

float glitch_beat(struct glitch *g) {
  return (g->frame - g->bpm_start) * g->bpm->value / 60.0 / SAMPLE_RATE;
}

float glitch_eval(struct glitch *g) {
  int apply_next = 1;
  /* If BPM is given - apply changes on the next beat */
  if (g->bpm->value > 0) {
    float beat = glitch_beat(g);
    if (beat - floorf(beat) > g->bpm->value / 60.0 / SAMPLE_RATE) {
      apply_next = 0;
    }
  }
  if (apply_next && g->next_expr != NULL) {
    if (g->bpm->value != g->last_bpm) {
      g->last_bpm = g->bpm->value;
      g->bpm_start = g->frame;
    }
    expr_destroy(g->e, NULL);
    g->e = g->next_expr;
    g->next_expr = NULL;
  }
  float v = expr_eval(g->e);
  if (!isnan(v)) {
    g->last_sample = v;
  }
  g->t->value = roundf(g->frame * 8000.0f / SAMPLE_RATE);
  g->frame++;
  return g->last_sample;
}
