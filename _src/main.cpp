#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#define _GLIBCXX_USE_NANOSLEEP /* Fixes sleep_for() in older gcc */
#include <thread>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include "vendor/oscpkt.hh"
#include "vendor/udp.hh"

#include "vendor/RtAudio.h"
#include "vendor/RtMidi.h"

#if !defined(_GLIBCXX_HAS_GTHREADS) && !defined(__APPLE__)
#include "vendor/mingw.mutex.h"
#include "vendor/mingw.thread.h"
#endif

#include "wav.h"

extern "C" {
#include "glitch.h"
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
  vec(char *) dirs = {0};

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

class Glitch {
public:
  Glitch() {
    g = glitch_create();
    play("");
  }

  ~Glitch() {
    closeMIDI();
    closeAudio();
    glitch_destroy(g);
  }

  int openAudio(int index = -1, unsigned int sampleRate = 44100,
                unsigned int numChannels = 2, unsigned int bufsz = 512) {
    int r = 0;
    m.lock();
    closeAudio();
    try {
      RtAudio::StreamParameters params;
      RtAudio::StreamOptions options;

      audio = new RtAudio();
      audio->showWarnings(false);
      if (index == -1) {
        index = audio->getDefaultOutputDevice();
        options.flags |= RTAUDIO_ALSA_USE_DEFAULT;
      }
      params.deviceId = index;
      params.nChannels = numChannels;
      this->numChannels = numChannels;

      glitch_sample_rate(sampleRate);

      audio->openStream(&params, NULL, RTAUDIO_FLOAT32, sampleRate, &bufsz,
                        [](void *out, void *in, unsigned int frames, double t,
                           RtAudioStreamStatus status, void *context) {
                          Glitch *g = (Glitch *)context;
                          float *buf = (float *)out;
                          if (status != 0) {
                            for (int i = 0; i < frames; i++) {
                              for (int j = 0; j < g->numChannels; j++) {
                                *buf++ = 0;
                              }
                            }
                            return 0;
                          }
                          g->m.lock();
                          for (int i = 0; i < frames; i++) {
                            float v = glitch_eval(g->g);
                            for (int j = 0; j < g->numChannels; j++) {
                              *buf++ = v;
                            }
                          }
                          g->m.unlock();
                          return 0;
                        },
                        this, &options);
      audio->startStream();

    } catch (RtAudioError &err) {
      std::cerr << err.what() << std::endl;
      closeAudio();
      r = -1;
    }
    m.unlock();
    return r;
  }

  void closeAudio() {
    m.lock();
    if (audio != NULL) {
      if (audio->isStreamOpen()) {
        audio->closeStream();
      }
      delete audio;
      audio = NULL;
    }
    m.unlock();
  }

  int openMIDI(std::set<int> indices = std::set<int>()) {
    int r = 0;
    m.lock();
    closeMIDI();
    RtMidiIn rt;
    try {
      for (int i = 0; i < rt.getPortCount(); i++) {
        if (indices.empty() || indices.find(i) != indices.end()) {
          RtMidiIn *m = new RtMidiIn();
          m->openPort(i);
          m->setCallback(
              [](double time, std::vector<unsigned char> *msg, void *arg) {
                Glitch *g = (Glitch *)arg;
                if (msg->size() == 3) {
                  g->midi(msg->at(0), msg->at(1), msg->at(2));
                }
              },
              this);
          midiInputs.push_back(m);
        }
      }
    } catch (RtAudioError &err) {
      std::cerr << err.what() << std::endl;
      r = -1;
    }
    m.unlock();
    return r;
  }

  void closeMIDI() {
    m.lock();
    for (auto m : midiInputs) {
      m->closePort();
      delete m;
    }
    midiInputs.clear();
    m.unlock();
  }

  int play(std::string s) {
    m.lock();
    int r = glitch_compile(g, s.c_str(), s.length());
    if (r == 0) {
      currentScript = s;
    }
    m.unlock();
    return r;
  }

  void midi(unsigned char cmd, unsigned char a, unsigned char b) {
    m.lock();
    glitch_midi(g, cmd, a, b);
    m.unlock();
  }

  static std::vector<std::string> listAudio() {
    std::vector<std::string> devices;
    try {
      RtAudio rt;
      rt.showWarnings(false);
      for (int i = 0; i < rt.getDeviceCount(); i++) {
        RtAudio::DeviceInfo info = rt.getDeviceInfo(i);
        devices.push_back(info.name);
      }
    } catch (RtAudioError &err) {
      std::cerr << err.getMessage() << std::endl;
    }
    return devices;
  }

  static std::vector<std::string> listMIDI() {
    std::vector<std::string> devices;
    try {
      RtMidiIn rt;
      for (int i = 0; i < rt.getPortCount(); i++) {
        devices.push_back(rt.getPortName(i));
      }
    } catch (RtAudioError &err) {
      std::cerr << err.getMessage() << std::endl;
    }
    return devices;
  }

private:
  std::recursive_mutex m;
  std::string currentScript;
  unsigned int numChannels;
  struct glitch *g = NULL;
  RtAudio *audio = NULL;
  std::vector<RtMidiIn *> midiInputs;
};

static void serverSendResult(oscpkt::UdpSocket *s, std::string uri, int r) {
  oscpkt::PacketWriter pw;
  oscpkt::Message result;
  result.init(uri).pushInt32(r);
  pw.init().addMessage(result);
  s->sendPacketTo(pw.packetData(), pw.packetSize(), s->packetOrigin());
}

static int clientSendCommand(oscpkt::UdpSocket &s, oscpkt::Message &req,
                             std::string res) {
  oscpkt::PacketWriter pw;
  pw.startBundle().startBundle().addMessage(req).endBundle().endBundle();
  if (!s.sendPacket(pw.packetData(), pw.packetSize())) {
    std::cerr << "client, send packet failed" << std::endl;
    return -1;
  }
  if (s.receiveNextPacket(30)) { // timeout, ms
    oscpkt::PacketReader pr(s.packetData(), s.packetSize());
    oscpkt::Message *m;
    while (pr.isOk() && (m = pr.popMessage()) != 0) {
      if (m->match(res)) {
        return 0;
      }
    }
  }
  std::cerr << "client, no response" << std::endl;
  return -2;
}

static void serverLoop(oscpkt::UdpSocket *s) {
  bool audioInitialized = false;
  bool midiInitialized = false;
  Glitch g;

  while (s->isOk()) {
    if (s->receiveNextPacket()) {
      oscpkt::PacketReader pr;
      pr.init(s->packetData(), s->packetSize());
      oscpkt::Message *msg;
      while (pr.isOk() && (msg = pr.popMessage()) != 0) {

        // Open another audio device
        if (msg->match("/glitch/settings/audio")) {
          int deviceID, sampleRate, numChannels, bufSize;
          if (msg->arg()
                  .popInt32(deviceID)
                  .popInt32(sampleRate)
                  .popInt32(numChannels)
                  .popInt32(bufSize)
                  .isOkNoMoreArgs()) {
            audioInitialized = true;
            int r = g.openAudio(deviceID, sampleRate, numChannels, bufSize);
            serverSendResult(s, "/glitch/status/audio", r);
            std::cerr << "opened audio devices: " << r << std::endl;
          }
        }

        // Open another set of MIDI devices
        if (msg->match("/glitch/settings/midi")) {
          std::set<int> ids;
          int r = 0;
          midiInitialized = true;
          auto arg = msg->arg();
          while (arg.nbArgRemaining() > 0) {
            if (!arg.isInt32()) {
              r = -1;
            } else {
              int id;
              arg = arg.popInt32(id);
              ids.insert(id);
            }
          }
          if (r == 0) {
            g.openMIDI(ids);
          }
          serverSendResult(s, "/glitch/status/midi", r);
          std::cerr << "opened midi devices: " << r << std::endl;
        }

        // Play another script
        if (msg->match("/glitch/play")) {
          std::string script;
          if (msg->arg().popStr(script).isOkNoMoreArgs()) {
            if (!audioInitialized) {
              audioInitialized = true;
              g.openAudio();
            }
            if (!midiInitialized) {
              midiInitialized = true;
              g.openMIDI();
            }
            int r = g.play(script);
            serverSendResult(s, "/glitch/status/play", r);
            std::cerr << "updated script: " << r << std::endl;
          }
        }
      }
    }
  }
  std::cerr << "server done: " << s->errorMessage() << std::endl;
}

static void listDevices(std::string prefix) {
  auto audio = Glitch::listAudio();
  std::cout << prefix << "Audio devices:" << std::endl;
  for (auto &s : audio) {
    std::cout << prefix << "  " << &s - &audio[0] << ": " << s << std::endl;
  }
  std::cout << std::endl;

  std::cout << prefix << "MIDI devices:" << std::endl;
  auto midi = Glitch::listMIDI();
  for (auto &s : midi) {
    std::cout << prefix << "  " << &s - &midi[0] << ": " << s << std::endl;
  }
  std::cout << std::endl;
}

static void usage(const char *app) {
  std::cout << std::endl;
  std::cout << "USAGE: " << app << " [options] <script>" << std::endl;
  std::cout << std::endl;
  std::cout << "    -d <device>  Audio device" << std::endl;
  std::cout << "    -b <frames>  Audio buffer size" << std::endl;
  std::cout << "    -r <rate>    Audio sample rate" << std::endl;
  std::cout << "    -n <chan>    Number of audio channels" << std::endl;
  std::cout << "    -m <device>  MIDI device(s)" << std::endl;
  std::cout << "    -w           Watch script file for changes" << std::endl;
  std::cout << "    -P <port>    listen on OSC/UDP port" << std::endl;
  std::cout << "    -p <port>    connect to OSC/UDP port" << std::endl;
  std::cout << std::endl;

  listDevices("  ");
}

int main(int argc, char *argv[]) {
  char opt;

  int audioDeviceID = -1;
  int sampleRate = 44100;
  int numChannels = 1;
  int bufSize = 1024;

  std::set<int> midiDeviceIDs;

  unsigned int serverPort = 0;
  unsigned int clientPort = 0;

  std::string filename = "";
  bool watch = false;

  bool hasAudioOptions = false;
  bool hasMIDIOptions = false;

  while ((opt = getopt(argc, argv, "d:r:n:b:m:p:P:wlh")) != -1) {
    switch (opt) {
    case 'd':
      hasAudioOptions = true;
      audioDeviceID = atoi(optarg);
      break;
    case 'm':
      hasMIDIOptions = true;
      midiDeviceIDs.insert(atoi(optarg));
      break;
    case 'b':
      hasAudioOptions = true;
      bufSize = atoi(optarg);
      break;
    case 'n':
      hasAudioOptions = true;
      numChannels = atoi(optarg);
      break;
    case 'r':
      hasAudioOptions = true;
      sampleRate = atoi(optarg);
      break;
    case 'p':
      clientPort = atoi(optarg);
      break;
    case 'P':
      serverPort = atoi(optarg);
      break;
    case 'w':
      watch = true;
      break;
    case 'l':
      listDevices("");
      exit(0);
    case 'h':
    default:
      usage(argv[0]);
      return 1;
    }
  }

  if (optind == argc - 1) {
    filename = argv[optind];
  }

  if (filename == "" && clientPort == 0 && serverPort == 0) {
    usage(argv[0]);
    return 1;
  }

  load_samples();

  oscpkt::UdpSocket server;
  std::thread *serverThread = NULL;

  if (serverPort != 0 && clientPort != 0 && serverPort != clientPort) {
    std::cerr
        << "both client and server ports are provided, but they don't match"
        << std::endl;
    usage(argv[0]);
    exit(1);
  }

  // If server port is provided or both ports are missing - start a server
  if (serverPort != 0 || (serverPort == 0 && clientPort == 0)) {
    server.bindTo(serverPort);
    if (!server.isOk()) {
      std::cerr << "failed to start server" << std::endl;
      exit(1);
    }
    std::cerr << "started server on port " << server.boundPort() << std::endl;
    clientPort = server.boundPort();
    serverThread = new std::thread(serverLoop, &server);
  }

  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  if (filename != "" || hasAudioOptions || hasMIDIOptions) {
    std::cerr << "starting client to port " << clientPort << std::endl;
    oscpkt::UdpSocket client;
    client.connectTo("localhost", clientPort);
    if (!client.isOk()) {
      std::cerr << "failed to start client" << std::endl;
      exit(1);
    }

    if (hasAudioOptions) {
      oscpkt::Message req("/glitch/settings/audio");
      req.pushInt32(audioDeviceID)
          .pushInt32(sampleRate)
          .pushInt32(numChannels)
          .pushInt32(bufSize);
      if (clientSendCommand(client, req, "/glitch/status/audio") < 0) {
        std::cerr << "failed to change audio settings" << std::endl;
        exit(1);
      }
    }

    if (hasMIDIOptions) {
      oscpkt::Message req("/glitch/settings/midi");
      for (int id : midiDeviceIDs) {
        req = req.pushInt32(id);
      }
      if (clientSendCommand(client, req, "/glitch/status/midi") < 0) {
        std::cerr << "failed to change MIDI settings" << std::endl;
        exit(1);
      }
    }

    if (filename != "") {
      time_t last_mtime = 0;
      do {
        struct stat st;
        if (stat(filename.c_str(), &st) == 0 && st.st_mtime != last_mtime) {
          last_mtime = st.st_mtime;
          try {
            std::ifstream ifs(filename);
            std::string script((std::istreambuf_iterator<char>(ifs)),
                               (std::istreambuf_iterator<char>()));
            oscpkt::Message req("/glitch/play");
            req.pushStr(script);
            if (clientSendCommand(client, req, "/glitch/status/play") < 0) {
              std::cerr << "failed to send script" << std::endl;
            } else {
              std::cerr << "script updated" << std::endl;
            }
          } catch (std::runtime_error &err) {
            std::cerr << err.what() << std::endl;
          } catch (std::exception &err) {
            std::cerr << err.what() << std::endl;
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
      } while (watch);
    }
  }

  if (serverThread != NULL) {
    serverThread->join();
  }
  return 0;
}
