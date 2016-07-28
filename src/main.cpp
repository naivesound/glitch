#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <dirent.h>

#include "sys.h"
#include "wav.h"
#include "RtAudio.h"
#include "RtMidi.h"

extern "C" {
#include "glitch.h"
}

static volatile sig_atomic_t sigusr = 0;
static volatile sig_atomic_t sigint = 0;
static mutex_t mutex;

static char *script_path = NULL;
static char *script = NULL;

static void sigusr_cb(int signo) { sigusr = 1; }

static void sigint_cb(int signo) { sigint = 1; }

static void midi_cb(double dt, std::vector<unsigned char> *msg, void *context) {
  static int last_note = -1;
  struct glitch *g = (struct glitch *)context;
  mutex_lock(&mutex);
  if (msg->size() == 3) {
    if (msg->at(0) == 144) {
      if (msg->at(2) > 0) {
        last_note = msg->at(1);
        glitch_xy(g, (float)msg->at(1), (float)msg->at(2));
      } else if (msg->at(1) == last_note) {
        glitch_xy(g, NAN, NAN);
      }
    } else if (msg->at(0) == 128 && msg->at(1) == last_note) {
      glitch_xy(g, NAN, NAN);
    }
  }
  mutex_unlock(&mutex);
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
  script = (char *)realloc(script, size + 1);
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
  struct glitch *g = (struct glitch *)context;
  float *buf = (float *)out;
  mutex_lock(&mutex);
  for (int i = 0; i < frames; i++) {
    float v = glitch_eval(g);
    /* 2 output channels: copy the same sample twice */
    *buf++ = v;
    *buf++ = v;
  }
  mutex_unlock(&mutex);
  return 0;
}

#define SAMPLES_DIR "samples"

struct wav_sample {
  const char *path;
  const char *name;
  int16_t *data;
  size_t len;
};

static vec(wav_sample) CACHE = {0};

static struct wav_sample *cache_find(const char *name, int variant) {
  int i;
  int start = -1;
  struct wav_sample it;

  vec_foreach(&CACHE, it, i) {
    if (strcmp(name, it.name) == 0) {
      start = i;
      break;
    }
  }
  if (start == -1) {
    return NULL;
  }
  for (i = start; i < vec_len(&CACHE); i++) {
    struct wav_sample *w = &vec_nth(&CACHE, i);
    if (strcmp(name, w->name) != 0) {
      return NULL;
    }
    if (i == start + variant) {
      return w;
    }
  }
  return NULL;
}

static float sample_loader(const char *name, int variant, int frame) {
  struct wav_sample *sample = cache_find(name, variant);
  if (sample == NULL) {
    return NAN;
  }
  if (sample->data == NULL) {
    FILE *f = wav_open(sample->path, &sample->len);
    if (f == NULL) {
      return NAN;
    }
    sample->data = (int16_t *)malloc(sample->len * sizeof(int16_t));
    wav_read(f, sample->data, sample->len);
    wav_close(f);
  }
  if (frame < sample->len) {
    return sample->data[frame] * 1.0f / 0x8000;
  }
  return NAN;
}

static int wav_sample_sort(const void *a, const void *b) {
  struct wav_sample *wa = (struct wav_sample *)a;
  struct wav_sample *wb = (struct wav_sample *)b;
  return strcmp(wa->path, wb->path);
}

static void load_samples() {
  struct dirent *e;

  glitch_set_loader(sample_loader);

  DIR *dir;
  struct dirent *dirent;
  vec(char *)dirs = {0};

  if ((dir = opendir(SAMPLES_DIR)) != NULL) {
    while ((dirent = readdir(dir)) != NULL) {
      if (dirent->d_name[0] != '.') {
        vec_push(&dirs, strdup(dirent->d_name));
      }
    }
    closedir(dir);
  }

  int i;
  char *it;
  vec_foreach(&dirs, it, i) {
    char dirpath[PATH_MAX];
    snprintf(dirpath, PATH_MAX - 1, "%s/%s", SAMPLES_DIR, it);
    if ((dir = opendir(dirpath)) != NULL) {
      while ((dirent = readdir(dir)) != NULL) {
        if (dirent->d_name[0] != '.') {
          char path[PATH_MAX];
          snprintf(dirpath, PATH_MAX - 1, "%s/%s/%s", SAMPLES_DIR, it,
                   dirent->d_name);
          struct wav_sample sample = {strdup(dirpath), strdup(it), NULL};
          vec_push(&CACHE, sample);
        }
      }
      closedir(dir);
      glitch_add_sample_func(it);
    }
  }
  vec_free(&dirs);

  qsort(&vec_nth(&CACHE, 0), vec_len(&CACHE), sizeof(wav_sample),
        wav_sample_sort);
}

static void usage(char *app) {
  fprintf(stderr, "Glitch " VERSION " - minimal algorithmic music maker\n\n"
                  "USAGE: %s [options ...] file\n\n"
                  "  -d <device>     Audio output device name\n"
                  /*"  -m <device>     MIDI-in device name\n"*/
                  "  -r <rate>       Audio sample rate\n"
                  "  -b <size>       Audio buffer size\n"
                  /*"  -w <file>       Write output to a WAV file\n"*/
                  "\n",
          app);
}

int main(int argc, char *argv[]) {
  int err = 0;
  int opt;
  struct glitch *g = NULL;
  char *device = NULL;
  char *midi = NULL;
  unsigned int bufsz = 512;

  int sample_rate = 48000;

  RtAudio *audio = NULL;
  RtMidiIn *midi_in = NULL;
  RtMidiOut *midi_out = NULL;
  RtAudio::StreamParameters params;
  RtAudio::StreamOptions options;
  RtAudio::DeviceInfo info;

  while ((opt = getopt(argc, argv, "d:m:b:r:h")) != -1) {
    switch (opt) {
    case 'd':
      device = optarg;
      break;
    case 'm':
      midi = optarg;
      break;
    case 'b':
      bufsz = atoi(optarg);
      break;
    case 'r':
      sample_rate = atoi(optarg);
      glitch_sample_rate(sample_rate);
      break;
    case 'h':
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind != argc - 1) {
    usage(argv[0]);
    return 1;
  }

  script_path = argv[optind];
  time_t last_mtime = mtime(script_path);

  load_samples();

  mutex_create(&mutex);

  g = glitch_create();
  glitch_compile(g, "", 0);

  reload(g);

  try {
    audio = new RtAudio();
    midi_in = new RtMidiIn();
    midi_out = new RtMidiOut();
  } catch (RtAudioError &error) {
    error.printMessage();
    err = 1;
    goto cleanup;
  }

  if (device == NULL) {
    params.deviceId = audio->getDefaultOutputDevice();
    options.flags |= RTAUDIO_ALSA_USE_DEFAULT;
    info = audio->getDeviceInfo(params.deviceId);
  } else {
    params.deviceId = -1;
    for (int i = 0; i < audio->getDeviceCount(); i++) {
      RtAudio::DeviceInfo info = audio->getDeviceInfo(i);
      if (info.name == device) {
        params.deviceId = i;
        break;
      }
    }
  }

  params.nChannels = 2;
  params.firstChannel = 0;

#if !defined(_WIN32)
  signal(SIGUSR1, sigusr_cb);
#endif
  signal(SIGINT, sigint_cb);

  midi_out->openVirtualPort("Glitch MIDI Output");

  if (midi != NULL) {
    for (int i = 0; i < midi_in->getPortCount(); i++) {
      std::string name = midi_in->getPortName(i);
      if (name == midi) {
        midi_in->openPort(i);
        midi_in->setCallback(&midi_cb, g);
        break;
      }
    }
  }

  try {
    audio->openStream(&params, NULL, RTAUDIO_FLOAT32, sample_rate, &bufsz,
                      glitch_cb, g, &options);
    audio->startStream();
  } catch (RtAudioError &error) {
    error.printMessage();
    err = 1;
    goto cleanup;
  }

  while (audio->isStreamRunning()) {
    sleep_ms(100);
    if (sigint) {
      audio->stopStream();
      break;
    }
    time_t t = mtime(script_path);
    if (sigusr || last_mtime != t) {
      sigusr = 0;
      last_mtime = t;
      reload(g);
    }
  }

cleanup:
  if (g != NULL) {
    glitch_destroy(g);
  }
  if (midi_in != NULL) {
    delete midi_in;
  }
  if (midi_out != NULL) {
    delete midi_out;
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
