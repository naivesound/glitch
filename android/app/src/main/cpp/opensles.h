#ifndef OPENSLES_H
#define OPENSLES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <stdlib.h>

#if 1
#include <android/log.h>
#define OPENSLES_LOG_TAG "OpenSLES-JNI"
#define OPENSLES_LOGD(...)                                                     \
  __android_log_print(ANDROID_LOG_DEBUG, OPENSLES_LOG_TAG, __VA_ARGS__)
#else
#define OPENSLES_LOGD(...)
#endif

struct opensles_engine {
  SLObjectItf engine_obj;
  SLEngineItf engine_itf;
  SLObjectItf output_obj;
  SLObjectItf player_obj;
  SLPlayItf player_itf;
  SLAndroidSimpleBufferQueueItf play_queue_itf;
};

static void opensles_close(struct opensles_engine *e);

static int opensles_open(struct opensles_engine *e) {
  SLresult r;

  e->engine_obj = e->output_obj = e->player_obj = NULL;

  r = slCreateEngine(&e->engine_obj, 0, NULL, 0, NULL, NULL);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: CreateEngine(): %d\n", r);
    goto fail;
  }

  r = (*e->engine_obj)->Realize(e->engine_obj, SL_BOOLEAN_FALSE);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: Realize(): %d\n", r);
    goto fail;
  }

  r = (*e->engine_obj)
	  ->GetInterface(e->engine_obj, SL_IID_ENGINE, &e->engine_itf);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: GetInterface(SL_IID_ENGINE): %d\n", r);
    goto fail;
  }

  r = (*e->engine_itf)
	  ->CreateOutputMix(e->engine_itf, &e->output_obj, 0, NULL, NULL);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: CreateOutputMix(): %d\n", r);
    goto fail;
  }

  r = (*e->output_obj)->Realize(e->output_obj, SL_BOOLEAN_FALSE);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: output Realize(): %d\n", r);
    goto fail;
  }

  OPENSLES_LOGD("opensles_open(): ok\n");
  return 0;

fail:
  opensles_close(e);
  return -1;
}

static int opensles_play(struct opensles_engine *e, int sample_rate,
			 int channels,
			 void (*cb)(SLAndroidSimpleBufferQueueItf, void *),
			 void *context) {
  SLresult r;
  int speakers =
      (channels > 1 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
		    : SL_SPEAKER_FRONT_CENTER);
  SLuint32 sr;
  switch (sample_rate) {
    sr = SL_SAMPLINGRATE_8;
    break;
  case 11025:
    sr = SL_SAMPLINGRATE_11_025;
    break;
  case 16000:
    sr = SL_SAMPLINGRATE_16;
    break;
  case 22050:
    sr = SL_SAMPLINGRATE_22_05;
    break;
  case 24000:
    sr = SL_SAMPLINGRATE_24;
    break;
  case 32000:
    sr = SL_SAMPLINGRATE_32;
    break;
  case 44100:
    sr = SL_SAMPLINGRATE_44_1;
    break;
  case 48000:
    sr = SL_SAMPLINGRATE_48;
    break;
  case 64000:
    sr = SL_SAMPLINGRATE_64;
    break;
  case 88200:
    sr = SL_SAMPLINGRATE_88_2;
    break;
  case 96000:
    sr = SL_SAMPLINGRATE_96;
    break;
  case 192000:
    sr = SL_SAMPLINGRATE_192;
    break;
  default:
    return -1;
  }
  SLDataFormat_PCM fmt_pcm = {SL_DATAFORMAT_PCM,
			      (SLuint32)channels,
			      sr,
			      SL_PCMSAMPLEFORMAT_FIXED_16,
			      SL_PCMSAMPLEFORMAT_FIXED_16,
			      (SLuint32)speakers,
			      SL_BYTEORDER_LITTLEENDIAN};
  SLDataLocator_AndroidSimpleBufferQueue loc_buf_queue = {
      SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 1};
  SLDataSource audio_src = {&loc_buf_queue, &fmt_pcm};

  SLDataLocator_OutputMix loc_out_mix = {SL_DATALOCATOR_OUTPUTMIX,
					 e->output_obj};
  SLDataSink audio_snk = {&loc_out_mix, NULL};

  const SLInterfaceID ids[] = {SL_IID_PLAY, SL_IID_BUFFERQUEUE};
  const SLboolean ids_req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
  r = (*e->engine_itf)
	  ->CreateAudioPlayer(e->engine_itf, &e->player_obj, &audio_src,
			      &audio_snk, 2, ids, ids_req);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: CreateAudioPlayer(): %d\n", r);
    goto fail;
  }

  r = (*e->player_obj)->Realize(e->player_obj, SL_BOOLEAN_FALSE);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: player Realize(): %d\n", r);
    goto fail;
  }

  r = (*e->player_obj)
	  ->GetInterface(e->player_obj, SL_IID_PLAY, &e->player_itf);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: player GetInterface(SL_IID_PLAY): %d\n", r);
    goto fail;
  }

  r = (*e->player_obj)
	  ->GetInterface(e->player_obj, SL_IID_BUFFERQUEUE, &e->play_queue_itf);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: player GetInterface(SL_IID_BUFFERQUEUE): %d\n", r);
    goto fail;
  }

  r = (*e->play_queue_itf)->RegisterCallback(e->play_queue_itf, cb, context);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: player RegisterCallback(): %d\n", r);
    goto fail;
  }

  r = (*e->player_itf)->SetPlayState(e->player_itf, SL_PLAYSTATE_PLAYING);
  if (r != SL_RESULT_SUCCESS) {
    OPENSLES_LOGD("error: player SetPlayState(SL_PLAYSTATE_PLAYING): %d\n", r);
    goto fail;
  }

  cb(e->play_queue_itf, context);

  OPENSLES_LOGD("opensles_play(): ok\n");
  return 0;

fail:
  opensles_close(e);
  return -1;
}

static void opensles_close(struct opensles_engine *e) {
  if (e->player_obj != NULL) {
    OPENSLES_LOGD("opensles_close: destroy player\n");
    (*e->player_obj)->Destroy(e->player_obj);
  }
  if (e->output_obj != NULL) {
    OPENSLES_LOGD("opensles_close: destroy output\n");
    (*e->output_obj)->Destroy(e->output_obj);
  }
  if (e->engine_obj != NULL) {
    OPENSLES_LOGD("opensles_close: destroy engine\n");
    (*e->engine_obj)->Destroy(e->engine_obj);
  }
  e->engine_obj = e->output_obj = e->player_obj = NULL;
}

#ifdef __cplusplus
}
#endif

#endif /* OPENSLES_H */
