#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android/log.h>

#include <jni.h>

#include <string>

#include "glitch.h"

#define LOG_TAG "Glitch-JNI"
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

struct opensles_engine {
    SLObjectItf engine_obj;
    SLEngineItf engine_itf;
    SLObjectItf output_obj;
    SLObjectItf player_obj;
    SLPlayItf player_itf;
    SLAndroidSimpleBufferQueueItf play_queue_itf;
} E;

static void opensles_close(struct opensles_engine *e);

static int opensles_open(struct opensles_engine *e) {
    SLresult r;

    e->engine_obj = e->output_obj = e->player_obj = NULL;

    r = slCreateEngine(&e->engine_obj, 0, NULL, 0, NULL, NULL);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: CreateEngine(): %d\n", r);
        goto fail;
    }

    r = (*e->engine_obj)->Realize(e->engine_obj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: Realize(): %d\n", r);
        goto fail;
    }

    r = (*e->engine_obj)
            ->GetInterface(e->engine_obj, SL_IID_ENGINE, &e->engine_itf);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: GetInterface(SL_IID_ENGINE): %d\n", r);
        goto fail;
    }

    r = (*e->engine_itf)
            ->CreateOutputMix(e->engine_itf, &e->output_obj, 0, NULL, NULL);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: CreateOutputMix(): %d\n", r);
        goto fail;
    }

    r = (*e->output_obj)->Realize(e->output_obj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: output Realize(): %d\n", r);
        goto fail;
    }

    LOGD("opensles_open(): ok\n");
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
    SLuint32 speakers =
        (channels > 1 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
                      : SL_SPEAKER_FRONT_CENTER);
    SLDataFormat_PCM fmt_pcm = {
        SL_DATAFORMAT_PCM,           (SLuint32)channels,
        (SLuint32)sample_rate,       SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16, speakers,
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
        LOGD("error: CreateAudioPlayer(): %d\n", r);
        goto fail;
    }

    r = (*e->player_obj)->Realize(e->player_obj, SL_BOOLEAN_FALSE);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: player Realize(): %d\n", r);
        goto fail;
    }

    r = (*e->player_obj)
            ->GetInterface(e->player_obj, SL_IID_PLAY, &e->player_itf);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: player GetInterface(SL_IID_PLAY): %d\n", r);
        goto fail;
    }

    r = (*e->player_obj)
            ->GetInterface(e->player_obj, SL_IID_BUFFERQUEUE,
                           &e->play_queue_itf);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: player GetInterface(SL_IID_BUFFERQUEUE): %d\n", r);
        goto fail;
    }

    r = (*e->play_queue_itf)->RegisterCallback(e->play_queue_itf, cb, context);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: player RegisterCallback(): %d\n", r);
        goto fail;
    }

    r = (*e->player_itf)->SetPlayState(e->player_itf, SL_PLAYSTATE_PLAYING);
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: player SetPlayState(SL_PLAYSTATE_PLAYING): %d\n", r);
        goto fail;
    }

    cb(e->play_queue_itf, context);

    LOGD("opensles_play(): ok\n");
    return 0;

fail:
    opensles_close(e);
    return -1;
}

static void opensles_close(struct opensles_engine *e) {
    if (e->player_obj != NULL) {
        LOGD("opensles_close: destroy player\n");
        (*e->player_obj)->Destroy(e->player_obj);
    }
    if (e->output_obj != NULL) {
        LOGD("opensles_close: destroy output\n");
        (*e->output_obj)->Destroy(e->output_obj);
    }
    if (e->engine_obj != NULL) {
        LOGD("opensles_close: destroy engine\n");
        (*e->engine_obj)->Destroy(e->engine_obj);
    }
    e->engine_obj = e->output_obj = e->player_obj = NULL;
}

static short static_buf[8192];

static void play_cb(SLAndroidSimpleBufferQueueItf q, void *p) {
    SLresult r;
    struct glitch *g = (struct glitch *)p;

    float start = (float)clock() / CLOCKS_PER_SEC;
    for (jint i = 0; i < sizeof(static_buf) / sizeof(static_buf[0]); i++) {
        float z = glitch_eval(g);
        static_buf[i] = (short)(z * 0x7fff);
    }
//    LOGD("fill: %f sec\n", ((float)clock() / CLOCKS_PER_SEC) - start);

    r = (*q)->Enqueue(q, static_buf, sizeof(static_buf));
    if (r != SL_RESULT_SUCCESS) {
        LOGD("error: Enqueue(): %d\n", r);
    }
}

JNIEXPORT jlong JNICALL Java_com_naivesound_glitch_Glitch_create(JNIEnv *env,
                                                                 jobject obj) {
    struct glitch *g = glitch_create();
    glitch_sample_rate(48000);
    glitch_compile(g, "", 0);
    LOGD("create(): %p\n", g);

    opensles_open(&E);
    opensles_play(&E, SL_SAMPLINGRATE_48, 1, play_cb, g);
#if 0
    typedef float (*glitch_loader_fn)(const char *name, int variant, int frame);
    void glitch_set_loader(glitch_loader_fn fn);
    int glitch_add_sample_func(const char *name);
#endif
    return (jlong)g;
}

JNIEXPORT void JNICALL Java_com_naivesound_glitch_Glitch_destroy(JNIEnv *env,
                                                                 jobject obj,
                                                                 jlong ref) {
    struct glitch *g = (struct glitch *)ref;
    LOGD("dispose(%p)", g);
    glitch_destroy(g);
}

JNIEXPORT jboolean JNICALL Java_com_naivesound_glitch_Glitch_compile(
    JNIEnv *env, jobject obj, jlong ref, jstring s) {
    struct glitch *g = (struct glitch *)ref;
    const char *script = env->GetStringUTFChars(s, 0);
    LOGD("compile(%p, %s)\n", g, script);
    const int r = glitch_compile(g, script, strlen(script));
    env->ReleaseStringUTFChars(s, script);
    return r == 0;
}

JNIEXPORT void JNICALL Java_com_naivesound_glitch_Glitch_fill(
    JNIEnv *env, jobject obj, jlong ref, jshortArray array, jint frames,
    jboolean stereo) {

    struct glitch *g = (struct glitch *)ref;
    for (jint i = 0; i < frames; i++) {
        float z = glitch_eval(g);
        if (stereo) {
            static_buf[i * 2] = static_buf[i * 2 + 1] = (short)(z * 0x7fff);
            i++;
        } else {
            static_buf[i] = (short)(z * 0x7fff);
        }
    }
    env->SetShortArrayRegion(array, 0, frames * (stereo ? 2 : 1), static_buf);
}

JNIEXPORT void JNICALL Java_com_naivesound_glitch_Glitch_setSampleRate(
    JNIEnv *env, jclass obj, jint sr) {
    glitch_sample_rate(sr);
}

#ifdef __cplusplus
}
#endif
