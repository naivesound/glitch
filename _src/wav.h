#ifndef WAV_H
#define WAV_H

struct riff_header {
  uint8_t riff_tag[4];
  uint32_t riff_length;
  uint8_t wave_tag[4];
};
struct fmt_data {
  uint16_t audio_format;
  uint16_t num_channels;
  uint32_t sample_rate;
  uint32_t byte_rate;
  uint16_t block_align;
  uint16_t bits_per_sample;
};
struct subchunk_header {
  uint8_t tag[4];
  uint32_t length;
};

struct wav_header {
  struct riff_header riff;
  struct subchunk_header fmt;
  struct fmt_data wave;
  struct subchunk_header data;
};

static FILE *wav_open(const char *path, size_t *datasz) {
  struct riff_header riff = {0};
  struct subchunk_header header = {0};

  FILE *f = fopen(path, "rb");
  if (f == NULL) {
    return NULL;
  }
  fread(&riff, sizeof(riff), 1, f);
  if (memcmp(riff.riff_tag, "RIFF", 4) != 0 ||
      memcmp(riff.wave_tag, "WAVE", 4) != 0) {
    fclose(f);
    return NULL;
  }
  do {
    size_t sz;
    sz = fread(&header, sizeof(header), 1, f);
    if (sz != 1) {
      break;
    }
    if (memcmp(header.tag, "fmt ", 4) == 0) {
      struct fmt_data fmt = {0};
      sz = fread(&fmt, sizeof(fmt), 1, f);
      if (sz != 1) {
	break;
      }
      if (fmt.num_channels != 1 || fmt.bits_per_sample != 16) {
	break;
      }
      fseek(f, header.length - sizeof(fmt), SEEK_CUR);
    } else if (memcmp(header.tag, "data", 4) == 0) {
      if (datasz != NULL) {
	*datasz = header.length / 2;
      }
      return f;
    } else {
      fseek(f, header.length, SEEK_CUR);
    }
  } while (true);
  fclose(f);
  return NULL;
}

static FILE *wav_create(const char *path, int rate) {
  struct wav_header header = {0};

  memcpy(header.riff.riff_tag, "RIFF", 4);
  memcpy(header.riff.wave_tag, "WAVE", 4);
  memcpy(header.fmt.tag, "fmt ", 4);
  memcpy(header.data.tag, "data", 4);

  header.riff.riff_length = 0;
  header.fmt.length = 16;
  header.wave.audio_format = 1;
  header.wave.num_channels = 1;
  header.wave.sample_rate = rate;
  header.wave.byte_rate = rate * 2;
  header.wave.block_align = 2;
  header.wave.bits_per_sample = 16;
  header.data.length = 0;

  FILE *f = fopen(path, "wb+");
  if (f == NULL) {
    return NULL;
  }
  fwrite(&header, sizeof(header), 1, f);
  fflush(f);
  return f;
}

static size_t wav_write(FILE *f, int16_t *data, size_t len) {
  return fwrite(data, sizeof(int16_t), len, f);
}

static size_t wav_read(FILE *f, int16_t *data, size_t len) {
  return fread(data, sizeof(int16_t), len, f);
}

static void wav_flush(FILE *file) {
  int file_length = ftell(file);

  int data_length = file_length - sizeof(struct wav_header);
  fseek(file, sizeof(struct wav_header) - sizeof(int), SEEK_SET);
  fwrite(&data_length, sizeof(data_length), 1, file);

  int riff_length = file_length - 8;
  fseek(file, 4, SEEK_SET);
  fwrite(&riff_length, sizeof(riff_length), 1, file);
}

static void wav_close(FILE *f) { fclose(f); }

#endif /* WAV_H */
