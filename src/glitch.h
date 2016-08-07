#ifndef GLITCH_H
#define GLITCH_H

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
  struct expr_var *v[MAX_POLYPHONY];
  char kq[MAX_POLYPHONY];

  long frame; /* Frame number since the beginning of the playback */
  long bpm_start; /* Frame number when tempo has been changed */
  float last_bpm;
  float last_sample;
};

struct glitch *glitch_create();
void glitch_destroy(struct glitch *g);
int glitch_compile(struct glitch *g, const char *s, size_t len);
float glitch_beat(struct glitch *g);
void glitch_xy(struct glitch *g, float x, float y);
void glitch_midi(struct glitch *g, unsigned char cmd, unsigned char a, unsigned char b);
float glitch_eval(struct glitch *g);

void glitch_sample_rate(int rate);

typedef float (*glitch_loader_fn)(const char *name, int variant, int frame);
void glitch_set_loader(glitch_loader_fn fn);
int glitch_add_sample_func(const char *name);

#endif /* GLITCH_H */
