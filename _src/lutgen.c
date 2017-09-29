#include <math.h>
#include <stdio.h>

#define PI 3.1415926f

int main() {
  printf("#ifndef MATH_LUT_H\n");
  printf("#define MATH_LUT_H\n");

  int sin_steps = 5000;
  printf("static float sin_lut[] = {\n");
  for (int i = 0; i < sin_steps; i++) {
    double w = i * 1.0 / sin_steps;
    printf("  %.10f, /* sin(%.5f) */\n", sin(w * 2 * PI), w);
  }
  printf("};\n");
  printf("#define SIN(x) (isnan(x) ? NAN : sin_lut[(int) ((x) * %d)])\n",
	 sin_steps);

  double pow_min = -5;
  double pow_max = 5;
  double pow_step = 1.0 / 48.0;
  printf("static float pow_lut[] = {\n");
  for (double x = pow_min; x <= pow_max; x = x + pow_step) {
    printf("  %.10f, /* pow(2, %.5f) */\n", pow(2, x), x);
  }
  printf("};\n");
  printf("static float POW2(float n) {\n");
  printf("  if (isnan(n) || n < %f || n > %f) {\n", pow_min, pow_max);
  printf("    return NAN;\n");
  printf("  }\n");
  printf("  return pow_lut[(int) (((n) - %f) / %f)];\n", pow_min, pow_step);
  printf("}\n");

  printf("#define LOG2(n) (logf(n) / logf(2))\n");

  printf("static float SQRT(float n) {\n");
  printf("  switch ((int) n) {\n");
  printf("  case 0: return 0;\n");
  printf("  case 1: return 1;\n");
  printf("  case 2: return 2;\n");
  printf("  case 3: return 2;\n");
  printf("  case 4: return 2;\n");
  printf("  case 5: return 2;\n");
  printf("  case 6: return 2;\n");
  printf("  case 7: return 3;\n");
  printf("  case 8: return 3;\n");
  printf("  case 9: return 3;\n");
  printf("  default: return 4;\n");
  printf("  }\n");
  printf("}\n");
  printf("#endif /* MATH_LUT_H */\n");
}
