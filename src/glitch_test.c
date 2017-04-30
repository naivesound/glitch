#include <assert.h>
#include <stdio.h>
#include <sys/time.h>

#include "glitch.c"

int status = 0;

#define ASSERT(expr)                                                           \
  do {                                                                         \
    if (!(expr)) {                                                             \
      printf("\nFAIL: %s (%d)\n\n", #expr, __LINE__);                          \
      status = 1;                                                              \
    }                                                                          \
  } while (0)

#define GLITCH_TEST(s)                                                         \
  for (struct glitch *g = glitch_create();                                     \
       g != NULL && glitch_compile(g, s, strlen(s)) == 0;                      \
       glitch_destroy(g), g = NULL)

static void test_r() {
  printf("TEST: r()\n");

  /* Should return values in the range [0..max) */
  GLITCH_TEST("r(10)") {
    for (int i = 0; i < 10; i++) {
      ASSERT(glitch_eval(g) >= 0 && glitch_eval(g) < 10);
    }
  }

  /* r(0) can only return 0 */
  GLITCH_TEST("r(0)") {
    for (int i = 0; i < 10; i++) {
      ASSERT(glitch_eval(g) == 0);
    }
  }

  /* Two sequential values are likely to be different */
  GLITCH_TEST("r()") { ASSERT(glitch_eval(g) != glitch_eval(g)); }

  /* By default r() returns values in the range [0..1) */
  GLITCH_TEST("r()") {
    for (int i = 0; i < 10; i++) {
      ASSERT(glitch_eval(g) >= 0 && glitch_eval(g) < 1);
    }
  }
}

static void test_hz() {
  printf("TEST: hz()\n");

  /* hz() == 440 */
  GLITCH_TEST("hz()") { ASSERT(glitch_eval(g) == 440.f); }

  /* hz(A4) == 440 */
  GLITCH_TEST("hz(A4)") { ASSERT(glitch_eval(g) == 440.f); }

  /* hz(A3) == 220 */
  GLITCH_TEST("hz(A3)") { ASSERT(glitch_eval(g) == 220.f); }

  /* hz(A3 + 0.5) > hz(A3), but < hz(A#3) */
  GLITCH_TEST("hz(A3+0.5)") {
    ASSERT(glitch_eval(g) > 220.f && glitch_eval(g) < 233.f);
  }
}

static void test_byte() {
  printf("TEST: byte()\n");

  /* byte() == 0 */
  GLITCH_TEST("byte()") { ASSERT(glitch_eval(g) == 0.f); }

  /* byte(0) ~= -1 */
  GLITCH_TEST("byte(0)") {
    ASSERT(glitch_eval(g) >= -1 && glitch_eval(g) < 0.99f);
  }

  /* byte(127) ~= 0 */
  GLITCH_TEST("byte(127)") { ASSERT(glitch_eval(g) == 0.f); }

  /* byte(255) ~= 1 */
  GLITCH_TEST("byte(255)") { ASSERT(glitch_eval(g) == 1.f); }

  /* byte(383) overflows and equals 0 */
  GLITCH_TEST("byte(383)") { ASSERT(glitch_eval(g) == 0.f); }

  /* byte(-257) overflows and equals 1 */
  GLITCH_TEST("byte(-257)") { ASSERT(glitch_eval(g) == 1.f); }
}

static void test_s() {
  printf("TEST: byte()\n");

  /* s() == 0 */
  GLITCH_TEST("s()") { ASSERT(glitch_eval(g) == 0.f); }

  /* s(0) == 0 */
  GLITCH_TEST("s(0)") { ASSERT(glitch_eval(g) == 0.f); }

  /* s(1) == 0 */
  GLITCH_TEST("s(1)") { ASSERT(glitch_eval(g) == 0.f); }

  /* s(0.25) == 1 */
  GLITCH_TEST("s(0.25)") { ASSERT(glitch_eval(g) == 1.f); }

  /* s(0.5) == 0 */
  GLITCH_TEST("s(0.5)") {
    ASSERT(glitch_eval(g) > -0.0001f && glitch_eval(g) < 0.00001f);
  }

  /* s(0.75) == 0 */
  GLITCH_TEST("s(0.75)") { ASSERT(glitch_eval(g) == -1.f); }
}

static void test_a() {
  printf("TEST: a()\n");

  /* a() == 0 */
  GLITCH_TEST("a()") { ASSERT(glitch_eval(g) == 0); }

  /* a(1) == 0 */
  GLITCH_TEST("a(1)") { ASSERT(glitch_eval(g) == 0); }

  /* a(1, 2, 3, 4) == 3 */
  GLITCH_TEST("a(1, 2, 3, 4)") { ASSERT(glitch_eval(g) == 3); }

  /* a(7, 2, 3, 4) == 3 */
  GLITCH_TEST("a(7, 2, 3, 4)") { ASSERT(glitch_eval(g) == 3); }

  /* a(-1, 2, 3, 4) == 4 */
  GLITCH_TEST("a(-1, 2, 3, 4)") { ASSERT(glitch_eval(g) == 4); }
}

static void test_seq() {
  printf("TEST: seq()\n");

  /* seq() returns NAN */
  GLITCH_TEST("seq() || 1") { ASSERT(glitch_eval(g) == 1); }
  /* seq(bpm) returns NAN */
  GLITCH_TEST("seq(120) || 1") { ASSERT(glitch_eval(g) == 1); }
  /* seq(NaN,...) returns NAN */
  GLITCH_TEST("seq(seq(),1,2,3) || -1") { ASSERT(glitch_eval(g) == -1); }

  /* seq(bpm, ...) switches values in a loop, returning NAN when beat ends */
  GLITCH_TEST("seq(60,1,2,3) || -1") {
    int prev_sr = SAMPLE_RATE;
    SAMPLE_RATE = 4;
    float expect[] = {
        1, 1, 1, -1, 2, 2, 2, -1, 3, 3, 3, -1,
        1, 1, 1, -1, 2, 2, 2, -1, 3, 3, 3, -1,
    };
    for (unsigned int i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
      ASSERT(abs(glitch_eval(g) - expect[i]) < 0.0001);
    }
    SAMPLE_RATE = prev_sr;
  }

  /* seq(bpm, ...) latches its value at the beginning of the beat */
  GLITCH_TEST("seq(60,r()) || -1") {
    int prev_sr = SAMPLE_RATE;
    SAMPLE_RATE = 4;
    float latched = glitch_eval(g);
    ASSERT(glitch_eval(g) == latched);
    ASSERT(glitch_eval(g) == latched);
    ASSERT(glitch_eval(g) == -1);
    SAMPLE_RATE = prev_sr;
  }

  /* If a tuple is passed as a step - step duration can be customized */
  GLITCH_TEST("seq(60,1,(1.5,2),3,(0.5,4)) || -1") {
    int prev_sr = SAMPLE_RATE;
    SAMPLE_RATE = 4;
    float expect[] = {
        1, 1, 1, -1, 2, 2, 2, 2, 2, -1, 3, 3, 3, -1, 4, -1,
        1, 1, 1, -1, 2, 2, 2, 2, 2, -1, 3, 3, 3, -1, 4, -1,
        1, 1, 1, -1, 2, 2, 2, 2, 2, -1, 3, 3, 3, -1, 4, -1,
        1, 1, 1, -1, 2, 2, 2, 2, 2, -1, 3, 3, 3, -1, 4, -1,
    };
    for (unsigned int i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
      ASSERT(abs(glitch_eval(g) - expect[i]) < 0.0001);
    }
    SAMPLE_RATE = prev_sr;
  }

  /* If more than 2 numbers are in a tuple - seq() should slide between the
   * values, returning NaN at the end of the whole step */
  GLITCH_TEST("seq(60,(3,1,4,5,2), 6) || -1") {
    int prev_sr = SAMPLE_RATE;
    SAMPLE_RATE = 4;
    float expect[] = {
        1, 1.75, 2.5, 3.25, 4, 4.25, 4.5, 4.75, 5, 4.25, 3.5, -1, 6, 6, 6, -1,
        1, 1.75, 2.5, 3.25, 4, 4.25, 4.5, 4.75, 5, 4.25, 3.5, -1, 6, 6, 6, -1,
        1, 1.75, 2.5, 3.25, 4, 4.25, 4.5, 4.75, 5, 4.25, 3.5, -1, 6, 6, 6, -1,
    };
    for (unsigned int i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
      ASSERT(abs(glitch_eval(g) - expect[i]) < 0.0001);
    }
    SAMPLE_RATE = prev_sr;
  }

  /* seq((start,bpm), ...) starts at a given step */
  GLITCH_TEST("seq((1,60),1,2,3) || -1") {
    int prev_sr = SAMPLE_RATE;
    SAMPLE_RATE = 4;
    float expect[] = {
        2, 2, 2, -1, 3, 3, 3, -1, 1, 1, 1, -1,
        2, 2, 2, -1, 3, 3, 3, -1, 1, 1, 1, -1,
    };
    for (unsigned int i = 0; i < sizeof(expect) / sizeof(expect[0]); i++) {
      ASSERT(abs(glitch_eval(g) - expect[i]) < 0.0001);
    }
    SAMPLE_RATE = prev_sr;
  }

  /* loop(bpm, ...) evaluates next value on each call */
  /* loop((start,bpm), ...) evaluates next value on each call */
  /* loop(bpm, (a,b)...) modifies the duration of each beat */
}

static void test_benchmark(const char *s) {
  struct timeval t;
  gettimeofday(&t, NULL);
  double start = t.tv_sec + t.tv_usec * 1e-6;
  struct glitch *g = glitch_create();
  if (g == NULL) {
    printf("FAIL: glitch instance can't be created\n");
    status = 1;
    return;
  }
  if (glitch_compile(g, s, strlen(s)) != 0) {
    printf("FAIL: %s can't be compiled\n", s);
    status = 1;
    return;
  }
  long N = 1000000L;
  for (long i = 0; i < N; i++) {
    glitch_eval(g);
  }
  gettimeofday(&t, NULL);
  double end = t.tv_sec + t.tv_usec * 1e-6;
  glitch_destroy(g);
  double ns = 1000000000 * (end - start) / N;
  printf("BENCH %40s:\t%f ns/op (%dM op/sec)\n", s, ns, (int)(1000 / ns));
}

static void run_benchmarks() {
  printf("\n## Instruments\n");
  test_benchmark("sin(440)");
  test_benchmark("saw(440)");
  test_benchmark("tri(440)");
  test_benchmark("sqr(440)");
  test_benchmark("fm(440,1,1)");
  test_benchmark("piano(440)");
  test_benchmark("pluck(440)");
  test_benchmark("tr808(BD,1)");

  printf("\n## Sequencers\n");
  test_benchmark("a(i=i+1,1,2,3,4)");
  test_benchmark("seq(120,1,2,3,4)");
  test_benchmark("seq(120,(0.5,1),2,(2,3),(1.5,4))");
  test_benchmark("seq((2,120),(0.5,1),2,(2,3),(1.5,4))");
  test_benchmark("seq(120*2,1,2,0,0,3,4,0,0)");
  test_benchmark("loop(120,1,2,3,4)");
  /* TODO: nested sequences */
  /* TODO: variable bpm and offset */

  printf("\n## Effects\n");
  test_benchmark("lpf(saw(440))");
  test_benchmark("hpf(saw(440))");
  test_benchmark("bpf(saw(440))");
  test_benchmark("bsf(saw(440))");
  test_benchmark("delay(piano(seq(120,440)),0.1,0.5,0.5)");

  /* TODO: variable delay */

  printf("\n## Utils\n");
  test_benchmark("hz(A4)");
  test_benchmark("scale(42)");
  test_benchmark("r()");
  test_benchmark("env(sin(seq(120,440)),(0.1,1),(0.3,0.1))");
  test_benchmark("mix(sin(220),sin(440),sin(880),sin(110))");
  test_benchmark("(sin(220)+sin(440)+sin(880)+sin(110))/4");
  test_benchmark("each(f,sin(f),220,440,880,110)/4");
}

int main() {
  test_r();
  test_hz();
  test_byte();
  test_s();
  test_a();
  test_seq();

  run_benchmarks();

  return status;
}
