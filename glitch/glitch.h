#ifndef GLITCH_H
#define GLITCH_H

#include "expr.h"

extern int SAMPLE_RATE;

struct glitch {
  int init;
  struct expr *e;
  struct expr *next_expr;
  struct expr_var_list vars;
  struct expr_var *t;
  struct expr_var *x;
  struct expr_var *y;
  struct expr_var *bpm;
  long frame;
  int measure;
  float last_sample;
};

struct glitch *glitch_create();
void glitch_destroy(struct glitch *g);
int glitch_compile(struct glitch *g, const char *s, size_t len);
void glitch_xy(struct glitch *g, float x, float y);
float glitch_eval(struct glitch *g);

void glitch_sample_rate(int rate);

#endif /* GLITCH_H */
