#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>

#include <android/log.h>

#include <jni.h>

#include <string>

#include "glitch.h"
#include "opensles.h"

struct glitch_context {
    pthread_mutex_t lock;
    struct opensles_engine E;
    struct glitch *glitch;
    int num_frames;
};

#define GLITCH_BUFSZ (4096 * 8)

#ifdef __cplusplus
extern "C" {
#endif

static void glitch_play_cb(SLAndroidSimpleBufferQueueItf q, void *p) {
    static short static_buf[GLITCH_BUFSZ];
    SLresult r;
    struct glitch_context *context = (struct glitch_context *)p;

    float start = (float)clock() / CLOCKS_PER_SEC;
    pthread_mutex_lock(&context->lock);

    for (jint i = 0; i < context->num_frames; i++) {
        float z = glitch_eval(context->glitch);
        static_buf[i * 2] = static_buf[i * 2 + 1] = (short)(z * 0x7fff);
    }
    pthread_mutex_unlock(&context->lock);

    r = (*q)->Enqueue(q, static_buf, 2 * context->num_frames * sizeof(short));
    if (r != SL_RESULT_SUCCESS) {
        OPENSLES_LOGD("error: Enqueue(): %d", r);
    }
}

JNIEXPORT jlong JNICALL Java_com_naivesound_glitch_Glitch_create(
    JNIEnv *env, jobject obj, jint sample_rate, jint num_frames) {
    struct glitch_context *context =
        (struct glitch_context *)malloc(sizeof(struct glitch_context));
    if (opensles_open(&context->E) < 0) {
        free(context);
        return 0;
    }
    pthread_mutex_init(&context->lock, NULL);

    context->glitch = glitch_create();
    if (num_frames < 32) {
        num_frames = 32;
    } else if (num_frames > GLITCH_BUFSZ / 2) {
        num_frames = GLITCH_BUFSZ / 2;
    }
    context->num_frames = num_frames;
    glitch_sample_rate(sample_rate);
    glitch_compile(context->glitch, "", 0);

    if (sample_rate == 48000) {
        opensles_play(&context->E, SL_SAMPLINGRATE_48, 2, glitch_play_cb,
                      context);
    } else {
        opensles_play(&context->E, SL_SAMPLINGRATE_44_1, 2, glitch_play_cb,
                      context);
    }
#if 0
    typedef float (*glitch_loader_fn)(const char *name, int variant, int frame);
    void glitch_set_loader(glitch_loader_fn fn);
    int glitch_add_sample_func(const char *name);
#endif
    return (jlong)context;
}

JNIEXPORT void JNICALL Java_com_naivesound_glitch_Glitch_destroy(JNIEnv *env,
                                                                 jobject obj,
                                                                 jlong ref) {
    struct glitch_context *context = (struct glitch_context *)ref;
    if (context == NULL) {
        return;
    }
    pthread_mutex_lock(&context->lock);
    opensles_close(&context->E);
    glitch_destroy(context->glitch);
    pthread_mutex_unlock(&context->lock);
    free(context);
}

JNIEXPORT jboolean JNICALL Java_com_naivesound_glitch_Glitch_compile(
    JNIEnv *env, jobject obj, jlong ref, jstring s) {
    struct glitch_context *context = (struct glitch_context *)ref;
    const char *script = env->GetStringUTFChars(s, 0);
    const int r = glitch_compile(context->glitch, script, strlen(script));
    env->ReleaseStringUTFChars(s, script);
    return r == 0;
}

#ifdef __cplusplus
}
#endif
