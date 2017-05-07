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

#define MAX_DELAY_TIME 10    /* seconds */
#define MIN_DELAY_BLOCK 8192 /* smallest delay buffer resize */

#ifdef GLITCH_USE_MATH
#include <math.h>
#define LOG2(n) (logf(n) / logf(2.f))
#define POW2(n) (powf(2.f, (n)))
#define SQRT(n) (sqrt(n))
#define SIN(n) (sinf((n)*2 * PI))
#else
#include "math_lut.h"
#endif

static float arg(vec_expr_t args, int n, float defval) {
  if (vec_len(&args) < n + 1) {
    return defval;
  }
  return expr_eval(&vec_nth(&args, n));
}

static inline float fwrap(float x) { return x - (long)x; }
static inline float fwrap2(float x) { return fwrap(fwrap(x) + 1); }
static inline float fsign(float x) { return (x < 0 ? -1 : 1); }
static inline float flim(float x, float a, float b) {
  if (!isnan(a) && x < a) {
    return a;
  } else if (!isnan(b) && x > b) {
    return b;
  }
  return x;
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

struct seq_step;
typedef vec(float) vec_float_t;
typedef vec(struct seq_step) vec_step_t;

struct pluck_context {
  int init;
  int t;
  float *sample;
};

struct seq_step {
  int start;
  int end;
  int gliss;
  float value;
  struct expr *e;
  struct expr *dur;
};

struct seq_context {
  int init;
  int offset;
  int t;
  int duration;
  int step;
  vec_step_t steps;
};

struct env_context {
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
  vec_float_t buf;
  int pos;
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
  float w = arg(args, 0, 0);
  return SIN(fwrap2(w));
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
    return LOG2(arg(args, 0, 0));
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
  return POW2(arg(args, 0, 0) / 12.f) * 440.f;
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
  return mix / SQRT(vec_len(&each->args));
}

static void lib_each_cleanup(struct expr_func *f, void *context) {
  (void)f;
  int i;
  struct expr e;
  struct each_context *each = (struct each_context *)context;
  vec_foreach(&each->args, e, i) { expr_destroy_args(&e); }
  vec_free(&each->args);
}

static float lib_osc(struct expr_func *f, vec_expr_t args, void *context) {
  struct osc_context *osc = (struct osc_context *)context;
  float freq = arg(args, 0, NAN);
  if (isnan(freq)) {
    return NAN;
  }
  float w = osc->w;
  osc->freq = freq;
  osc->w = fwrap(osc->w + osc->freq / SAMPLE_RATE);
  if (strncmp(f->name, "sin", 4) == 0) {
    return SIN(fwrap2(w));
  } else if (strncmp(f->name, "tri", 4) == 0) {
    float sign = fsign(w);
    w = fwrap(w * sign + 0.25f) - 0.5f;
    return sign * 4 * (0.25f - w * fsign(w));
  } else if (strncmp(f->name, "saw", 4) == 0) {
    return 2 * fwrap(w + 0.5f * fsign(w)) - fsign(w);
  } else if (strncmp(f->name, "sqr", 4) == 0) {
    w = fwrap2(w);
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

  float v3 = mi3 * SIN(fm->w3);
  float v2 = mi2 * SIN(fm->w2);
  float v1 = mi1 * SIN(fwrap2(fm->w1 + v3));
  float v0 = SIN(fwrap2(fm->w0 + v1 + v2));

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

  /* At least BPM and one step should be provided */
  if (vec_len(&args) < 2) {
    return NAN;
  }

  /* Initialize vector of steps, cache all expression pointers */
  if (!seq->init) {
    seq->init = 1;
    for (int i = 1; i < vec_len(&args); i++) {
      struct expr *e = &vec_nth(&args, i);
      struct expr *dur = NULL;
      if (e->type == OP_COMMA) {
        dur = &vec_nth(&e->param.op.args, 0);
        e = &vec_nth(&e->param.op.args, 1);
      }
      struct seq_step step = {.gliss = 0, .e = e, .dur = dur};
      if (e->type == OP_COMMA) {
        int gliss = 0;
        for (struct expr *sube = e; sube->type == OP_COMMA;
             sube = &vec_nth(&sube->param.op.args, 1)) {
          gliss++;
        }
        while (e->type == OP_COMMA) {
          step.e = &vec_nth(&e->param.op.args, 0);
          step.gliss = gliss;
          vec_push(&seq->steps, step);
          e = &vec_nth(&e->param.op.args, 1);
          step.gliss = -1;
        }
        step.e = e;
      }
      vec_push(&seq->steps, step);
    }
  }

  /* A function can be either "seq" or "loop" */
  int is_seq = (strncmp(f->name, "seq", 4) == 0);

  /* Sequencer reached the end of loop. Re-calculate step durations, initial
   * offset, cache step values if needed */
  if (seq->t == 0) {
    struct expr *bpm = &vec_nth(&args, 0);
    float offset = 0;
    if (bpm->type == OP_COMMA) {
      offset = expr_eval(&vec_nth(&bpm->param.op.args, 0));
    }
    float tempo = expr_eval(bpm);
    if (isnan(tempo) || tempo <= 0) {
      return NAN;
    }

    float samples_per_beat = SAMPLE_RATE / (tempo / 60.f);

    seq->offset = (int)(offset * samples_per_beat);

    int t = 0;

    struct expr *prev_dur = NULL;
    float prev_dur_value = 1.f;

    seq->step = -1;
    for (int i = 0; i < vec_len(&seq->steps); i++) {
      struct seq_step *step = &vec_nth(&seq->steps, i);
      if (step->dur != prev_dur) {
        prev_dur = step->dur;
        prev_dur_value = (prev_dur == NULL ? 1.f : expr_eval(prev_dur));
      }
      float dur = prev_dur_value;
      int gliss_len = (step->gliss > 0 ? step->gliss : 1);
      step->start = t;
      if (seq->step == -1 && t >= seq->offset) {
        seq->step = i;
      }
      if (step->gliss >= 0) {
        t += (int)(dur * samples_per_beat / gliss_len);
        step->end = t;
      } else {
        step->end = step->start;
      }
      seq->duration = step->end;
      if (is_seq) {
        step->value = expr_eval(step->e);
      }
    }
  }

  /* Prepare to return the value of the current step */
  int t = (seq->t + seq->offset) % seq->duration;
  struct seq_step *step = &vec_nth(&seq->steps, seq->step);
  float v = NAN;

  /* Advance step if needed */
  int loop = seq->step;
  while (t >= step->end || (t == 0 && step->end == seq->duration)) {
    seq->step = (seq->step + 1) % vec_len(&seq->steps);
    step = &vec_nth(&seq->steps, seq->step);
    if (seq->step == loop) {
      break;
    }
  }

  int has_gliss = (step->gliss > 0);

  /* Get step value either from cache (seq) or by evaluating current step
   * expression (loop) */
  if (t < step->end - 1 || (has_gliss && t < step->end)) {
    if (is_seq) {
      /* Return step value */
      if (step->gliss == 0) {
        v = step->value;
      } else {
        struct seq_step *next_step = &vec_nth(&seq->steps, seq->step + 1);
        if (t == (step->end - 1) && next_step->gliss == -1) {
          v = NAN;
        } else {
          float progress = 1.f * (t - step->start) / (step->end - step->start);
          float a = step->value;
          float b = next_step->value;
          v = a + (b - a) * progress;
        }
      }
    } else {
      v = expr_eval(step->e);
    }
  }

  seq->t = (seq->t + 1) % seq->duration;
  return v;
}

static void lib_seq_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct seq_context *seq = (struct seq_context *)context;
  vec_free(&seq->steps);
}

#define calc_env_exp(t, c, mult, diff)                                         \
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

static float lib_env(struct expr_func *f, vec_expr_t args, void *context) {
  (void)f;
  struct env_context *env = (struct env_context *)context;

  float v = arg(args, 0, NAN);
  if (isnan(v)) {
    env->t = 0;
  }

  if (env->t == 0) {
    env->at = (int)(arg(args, 1, 0.01) * SAMPLE_RATE);
    env->rt = (int)(arg(args, 2, env->at * 10) * SAMPLE_RATE);
    float ac = flim(arg(args, 3, 0.5), 0.0001f, 0.9999f);
    float rc = flim(arg(args, 4, ac), 0.0001f, 0.9999f);

    calc_env_exp(env->at, ac, env->amul, env->adif);
    calc_env_exp(env->rt, rc, env->rmul, env->rdif);
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
  if (isnan(v)) {
    env->t = 0;
  }

  return r * v;
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
    float sample = expr_eval(e);
    if (isnan(sample)) {
      sample = vec_nth(&mix->values, i);
    }
    vec_nth(&mix->values, i) = sample;
    v = v + sample;
  }
  if (vec_len(&args) > 0) {
    v = v / SQRT(vec_len(&args));
    if (v <= -1.25f) {
      return -0.984375;
    } else if (v >= 1.25f) {
      return 0.984375;
    } else {
      return 1.1f * v - 0.2f * v * v * v;
    }
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
  if (cutoff <= 0 || q <= 0) {
    return 0;
  }

  float w0 = cutoff / SAMPLE_RATE;
  float cs = SIN(fwrap(w0 + 0.25));
  float sn = SIN(fwrap(w0));
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
  if (time <= 0) {
    return signal;
  }
  if (time > MAX_DELAY_TIME) {
    time = MAX_DELAY_TIME;
  }

  int bufsz = (int)(time * SAMPLE_RATE);

  /* Expand buffer if needed */
  if (vec_len(&delay->buf) < bufsz) {
    int sz = ((bufsz / MIN_DELAY_BLOCK) + 1) * MIN_DELAY_BLOCK;
    int i = vec_len(&delay->buf);
    delay->buf.len = sz;
    delay->buf.cap = sz;
    vec_expand(vec_unpack(&delay->buf));
    while (i < sz) {
      vec_nth(&delay->buf, i++) = 0;
    }
  }

  /* Get value delayed value from the buffer */
  float out = 0;
  if (bufsz <= vec_len(&delay->buf)) {
    unsigned int i =
        (delay->pos + vec_len(&delay->buf) - bufsz) % vec_len(&delay->buf);
    out = vec_nth(&delay->buf, i) * level;
  }

  /* Write updated value to the buffer */
  vec_nth(&delay->buf, delay->pos) =
      vec_nth(&delay->buf, delay->pos) * feedback + signal;
  delay->pos = (delay->pos + 1) % vec_len(&delay->buf);
  return signal + out;
}

static void lib_delay_cleanup(struct expr_func *f, void *context) {
  (void)f;
  struct delay_context *delay = (struct delay_context *)context;
  vec_free(&delay->buf);
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
    sample->t = sample->t + POW2(shift / 12.0);
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
  float note = 12.f * LOG2(freq / 440.f);
  float base_freq =
      (freq < 130.f ? 65.41f : (freq < 523.f ? 261.63f : 1046.50f));
  float base_note = 12.f * LOG2(base_freq / 440.f);
  float shift = (note - base_note);
  unsigned char *pcm = samples[index];
  if (sample->t * 2 + 0x80 + 1 < len[index]) {
    unsigned char hi = pcm[0x80 + (int)sample->t * 2 + 1];
    unsigned char lo = pcm[0x80 + (int)sample->t * 2];
    float x = int16_sample(hi, lo);
    sample->t = sample->t + POW2(shift / 12.0);
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
  sample->t = sample->t + POW2(shift / 12.0);
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

    {"env", lib_env, NULL, sizeof(struct env_context)},

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
        g->g[i]->value = b / 128.0;
        g->v[i]->value = b / 128.0;
        break;
      }
    }
  } else if ((cmd == 0x9 && b == 0) || cmd == 0x8) {
    // Note released: remove from the "list" and shift the rest
    float key = a - 69;
    for (int i = 0; i < MAX_POLYPHONY; i++) {
      if (g->k[i]->value == key) {
        g->g[i]->value = NAN;
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
      snprintf(name, sizeof(name), "g%d", i);
      g->g[i] = expr_var(&g->vars, name, strlen(name));
      g->k[i]->value = g->v[i]->value = g->g[i]->value = NAN;
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
  g->t->value = (long)(g->frame * 8000.0f / SAMPLE_RATE);
  g->frame++;
  for (int i = 0; i < MAX_POLYPHONY; i++) {
    if (!isnan(g->v[i]->value) && isnan(g->g[i]->value)) {
      g->v[i]->value = g->v[i]->value * 0.999;
      if (g->v[i]->value < 0.01f) {
        g->v[i]->value = NAN;
        g->k[i]->value = NAN;
      }
    }
  }
  return g->last_sample;
}
