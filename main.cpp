#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "sys.h"
#include "RtAudio.h"

extern "C" {
  #include "glitch.h"
}

static volatile sig_atomic_t sig = 0;
static mutex_t mutex;
static char *script_path = NULL;
static char *script = NULL;

static void sighup_cb(int signo) {
  sig = 1;
}

static void reload(struct glitch *g) {
  long size;
  FILE *f = fopen(script_path, "r");
  if (f == NULL) {
    fprintf(stderr, "glitch: failed to open %s\n", script_path);
    return;
  }
  fseek(f, 0, SEEK_END);
  size = ftell(f);
  fseek(f, 0, SEEK_SET);
  script = (char *) realloc(script, size + 1);
  fread(script, 1, size, f);
  fclose(f);
  script[size] = '\0';

  mutex_lock(&mutex);
  if (glitch_compile(g, script, strlen(script)) < 0) {
    fprintf(stderr, "glitch: syntax error\n");
  }
  mutex_unlock(&mutex);
}

static int glitch_cb(void *out, void *in, unsigned int frames, double t,
    RtAudioStreamStatus status, void *context) {
  struct glitch *g = (struct glitch *) context;
  float *buf = (float *) out;
  mutex_lock(&mutex);
  for (int i = 0; i < frames; i++) {
    float v = glitch_eval(g);
    *buf++ = v;
  }
  mutex_unlock(&mutex);
  return 0;
}

/*
 * TODO: CLI options:
 *   -d <device>
 *   -r <sample rate>
 *   -b <bufsz>
 *   -m <midi input>
 *   -w <wav file>
 *   <file>
 */
int main(int argc, char *argv[]) {
  int err = 0;
  RtAudio *audio = NULL;
  struct glitch *g = NULL;
  unsigned int device = -1;
  unsigned int bufsz = 1024;
  time_t last_mtime = -1;
  RtAudio::StreamParameters params;
  RtAudio::StreamOptions options;

  if (argc != 2) {
    fprintf(stderr, "glitch <file>\n");
    return 1;
  }

  script_path = argv[1];
  mutex_create(&mutex);
  
  try {
    audio = new RtAudio();
  } catch (RtAudioError &error) {
    error.printMessage();
    err = 1;
    goto cleanup;
  }


  if (device == -1) {
    device = audio->getDefaultOutputDevice();
    options.flags |= RTAUDIO_ALSA_USE_DEFAULT;
  }
  params.deviceId = device;
  params.nChannels = 1;
  params.firstChannel = 0;

  signal(SIGHUP, sighup_cb);

  g = glitch_create();
  glitch_compile(g, "", 0);

  reload(g);

  try {
    audio->openStream(&params, NULL, RTAUDIO_FLOAT32, SAMPLE_RATE, &bufsz,
	glitch_cb, g, &options);
    audio->startStream();
  } catch (RtAudioError &error) {
    error.printMessage();
    err = 1;
    goto cleanup;
  }

  while (audio->isStreamRunning()) {
    sleep_ms(100);
    time_t t = mtime(argv[1]);
    if (sig || last_mtime != t) {
      sig = 0;
      last_mtime = t;
      reload(g);
    }
  }

cleanup:
  if (g != NULL) {
    glitch_destroy(g);
  }
  if (audio != NULL) {
    if (audio->isStreamOpen()) {
      audio->closeStream();
    }
    delete audio;
  }
  mutex_destroy(&mutex);
  return err;
}

