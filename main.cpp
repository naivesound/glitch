#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>

#include "sys.h"
#include "RtAudio.h"
#include "RtMidi.h"

extern "C" {
  #include "glitch.h"
}

static volatile sig_atomic_t sighup = 0;
static volatile sig_atomic_t sigint = 0;
static mutex_t mutex;

static char *script_path = NULL;
static char *script = NULL;

static void sighup_cb(int signo) {
  sighup = 1;
}

static void sigint_cb(int signo) {
  sigint = 1;
}

static void midi_cb(double dt, std::vector<unsigned char> *msg, void *context) {
  static int last_note = -1;
  struct glitch *g = (struct glitch *) context;
  mutex_lock(&mutex);
  if (msg->size() == 3) {
    if (msg->at(0) == 144) {
      last_note = msg->at(1);
      glitch_xy(g, (float) msg->at(1), (float) msg->at(2));
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
    /* 2 output channels: copy the same sample twice */
    *buf++ = v;
    *buf++ = v;
  }
  mutex_unlock(&mutex);
  return 0;
}

static void usage(char *app) {
  fprintf(stderr, "USAGE: %s [options ...] file\n\n"
      "  -d <device>     Audio output device name\n"
      "  -m <device>     MIDI-in device name\n"
      "  -r <rate>       Audio sample rate\n"
      "  -b <size>       Audio buffer size\n"
      /*"  -w <file>       Write output to a WAV file\n"*/
      "\n", app);
}

int main(int argc, char *argv[]) {
  int err = 0;
  int opt;
  struct glitch *g = NULL;
  char *device = NULL;
  char *midi = NULL;
  unsigned int bufsz = 256;

  time_t last_mtime = -1;

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
	SAMPLE_RATE = atoi(optarg);
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

  signal(SIGHUP, sighup_cb);
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
    if (sigint) {
      audio->stopStream();
      break;
    }
    time_t t = mtime(argv[1]);
    if (sighup || last_mtime != t) {
      sighup = 0;
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

