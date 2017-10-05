#ifndef GLITCH_H
#define GLITCH_H

#ifdef __cplusplus
extern "C" {
#endif
#include "expr.h"

#define MAX_POLYPHONY 9

struct glitch {
  int init;
  struct expr *e;
  struct expr *next_expr;
  struct expr_var_list vars;
  struct expr_var *t;
  struct expr_var *x;
  struct expr_var *y;
  struct expr_var *bpm;

  struct expr_var *k[MAX_POLYPHONY];
  struct expr_var *g[MAX_POLYPHONY];
  struct expr_var *v[MAX_POLYPHONY];

  long frame;     /* Frame number since the beginning of the playback */
  long bpm_start; /* Frame number when tempo has been changed */
  float last_bpm;
  float last_sample;
};

typedef float (*glitch_loader_fn)(const char *name, int variant, int frame);

void glitch_init(int sample_rate, unsigned long long seed);
void glitch_set_sample_loader(glitch_loader_fn fn);
int glitch_add_sample(const char *name);
int glitch_remove_sample(const char *name);

struct glitch *glitch_create();
void glitch_destroy(struct glitch *g);
int glitch_compile(struct glitch *g, const char *s, size_t len);
void glitch_reset(struct glitch *g);
void glitch_set(struct glitch *g, const char *name, float value);
float glitch_get(struct glitch *g, const char *name);
void glitch_midi(struct glitch *g, unsigned char cmd, unsigned char a,
                 unsigned char b);
void glitch_fill(struct glitch *g, float *buf, size_t frames, size_t channels);

#ifdef __cplusplus
}
#endif

#endif /* GLITCH_H */
