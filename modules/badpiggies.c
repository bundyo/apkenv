
/**
 * apkenv
 * Copyright (c) 2012, Thomas Perl <m@thp.io>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **/

/**
 * Bad Piggies support module 0.1 
 * by Bundyo
 **/

#ifdef APKENV_DEBUG
#  define MODULE_DEBUG_PRINTF(...) printf(__VA_ARGS__)
#else
#  define MODULE_DEBUG_PRINTF(...)
#endif

#include "common.h"

#include <string.h>
#include <limits.h>
#include "SDL/SDL.h"
#include <assert.h>

// Typedefs. Got these from classes.dex (http://stackoverflow.com/questions/1249973/decompiling-dex-into-java-sourcecode)
typedef void (*jni_init_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*badpiggies_init_t)(JNIEnv *env, jobject obj, jint paramInt1, jint paramInt2) SOFTFP;
typedef void (*badpiggies_input_t)(JNIEnv *env, jobject obj, jint action, jfloat x, jfloat y, jint finger, jlong paramLong, jint paramInt) SOFTFP;
typedef jboolean (*badpiggies_update_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef jboolean (*badpiggies_pause_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*badpiggies_resume_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*badpiggies_deinit_t)(JNIEnv *env, jobject obj) SOFTFP;

struct SupportModulePriv {
    jni_onload_t JNI_OnLoad;
    jni_init_t initJni;
    badpiggies_init_t native_init;
    badpiggies_update_t native_update;
    badpiggies_input_t native_input;
    badpiggies_pause_t native_pause;
    badpiggies_resume_t native_resume;
    badpiggies_deinit_t native_deinit;
};
static struct SupportModulePriv badpiggies_priv;

/* Audio specs and handle */
SDL_AudioSpec *desired, *obtained;
jlong audioHandle;

/* Global application state so we can call this from override thingie */
struct GlobalState *global;

char *badpiggies_libname = "libunity.so";

/* Fill audio buffer */
void my_audio_callback(void *ud, Uint8 *stream, int len)
{
//    badpiggies_priv.native_mixdata(ENV(global), VM(global), audioHandle, stream, len);
}


/* CallVoidMethodV override. Signal when to start or stop audio */
static void
badpiggies_CallVoidMethodV(JNIEnv* p0, jobject p1, jmethodID p2, va_list p3)
{
    MODULE_DEBUG_PRINTF("module_JNIEnv_CallVoidMethodV(%x, %s, %s)\n", p1, p2->name, p2->sig);
    if (strcmp(p2->name, "startOutput") == 0)
    {
        MODULE_DEBUG_PRINTF("Start audio Output\n");
        SDL_PauseAudio(0);
    }
    if (strcmp(p2->name, "stopOutput") == 0)
    {
        // Stop output :)
        MODULE_DEBUG_PRINTF("Stop audio Output\n");
        SDL_PauseAudio(1);
    }
}

/* NewObjectV override. Initialize audio output */
static jobject
badpiggies_NewObjectV(JNIEnv *env, jclass p1, jmethodID p2, va_list p3)
{
    struct dummy_jclass *clazz = p1;
    MODULE_DEBUG_PRINTF("module_JNIEnv_NewObjectV(%x, %s, %s)\n", p1, p2->name, clazz->name);
    if (strcmp(clazz->name, "org/fmod/FMODAudioDevice") == 0)
    {
        /* Open the audio device */
        desired = malloc(sizeof(SDL_AudioSpec));
        obtained = malloc(sizeof(SDL_AudioSpec));

        audioHandle = va_arg(p3, jlong);

        desired->freq = va_arg(p3, int);
        desired->channels = va_arg(p3, int);
        desired->format = AUDIO_S16SYS;
        jint bitrate = va_arg(p3, int); 
        desired->samples = va_arg(p3, int) / 8;
        desired->callback=my_audio_callback;
        desired->userdata=NULL;
        MODULE_DEBUG_PRINTF("Module: Handle: %lld, freq: %i, channels: %i, bitrate: %i, samples: %i\n",
            audioHandle,desired->freq,desired->channels,bitrate,desired->samples);

        assert( SDL_OpenAudio(desired, obtained) == 0 );
        free(desired);
    }

    return GLOBAL_J(env);
}

/* CallObjectMethodV override. AB calls readFile to read data from apk */
static jobject
badpiggies_CallObjectMethodV(JNIEnv *env, jobject p1, jmethodID p2, va_list p3)
{
    MODULE_DEBUG_PRINTF("module_JNIEnv_CallObjectMethodV(%x, %s, %s, ...)\n", p1, p2->name, p2->sig);
    if (strcmp(p2->name, "readFile") == 0)
    {
        struct dummy_jstring *str = va_arg(p3,struct dummy_jstring*);
        char tmp[PATH_MAX];
        strcpy(tmp, "assets/");
        strcat(tmp, str->data);

        size_t file_size;
	struct dummy_byte_array *result = malloc(sizeof(struct dummy_byte_array));

        global->read_file(tmp, &result->data, &file_size);

        result->size = file_size;
        return result;
    }
    return NULL;
}

/* DeleteLocalRef override. Free some memory :) */
static void
badpiggies_DeleteLocalRef(JNIEnv* p0, jobject p1)
{
    MODULE_DEBUG_PRINTF("JNIEnv_DeleteLocalRef(%x)\n", p1);
    if (p1 == GLOBAL_J(p0) || p1 == NULL) {
        MODULE_DEBUG_PRINTF("WARNING: DeleteLocalRef on global\n");
        return;
    }
    free(p1);
}

static int
badpiggies_try_init(struct SupportModule *self)
{
    self->priv->initJni = (jni_init_t)LOOKUP_M("initJni");
    self->priv->JNI_OnLoad = (jni_onload_t)LOOKUP_M("JNI_OnLoad");
    self->priv->native_init = (badpiggies_init_t)LOOKUP_M("nativeInit");
    self->priv->native_input = (badpiggies_input_t)LOOKUP_M("nativeTouch");
    self->priv->native_update = (badpiggies_update_t)LOOKUP_M("nativeRender");
    self->priv->native_pause = (badpiggies_pause_t)LOOKUP_M("nativePause");
    self->priv->native_resume = (badpiggies_resume_t)LOOKUP_M("nativeResume");
    self->priv->native_deinit = (badpiggies_deinit_t)LOOKUP_M("nativeDone");

    //printf("initJni: %s\n", self->priv->initJni ? "true" : "false");
    printf("OnLoad: %s\n", self->priv->JNI_OnLoad ? "true" : "false");
    printf("init: %s\n", self->priv->native_init ? "true" : "false");
    printf("input: %s\n", self->priv->native_input ? "true" : "false");
    printf("update: %s\n", self->priv->native_update ? "true" : "false");
    printf("pause: %s\n", self->priv->native_pause ? "true" : "false");
    printf("resume: %s\n", self->priv->native_resume ? "true" : "false");
    printf("deinit: %s\n", self->priv->native_deinit ? "true" : "false");

    /* Overrides for JNIEnv_ */
    self->override_env.CallObjectMethodV = badpiggies_CallObjectMethodV;
    self->override_env.DeleteLocalRef = badpiggies_DeleteLocalRef;
    self->override_env.CallVoidMethodV = badpiggies_CallVoidMethodV;
    self->override_env.NewObjectV = badpiggies_NewObjectV;

    return (self->priv->initJni != NULL &&
            self->priv->JNI_OnLoad != NULL &&
            self->priv->native_init != NULL &&
            self->priv->native_input != NULL &&
            self->priv->native_update != NULL &&
            self->priv->native_pause != NULL &&
            self->priv->native_resume != NULL &&
            self->priv->native_deinit != NULL);
}

static void
badpiggies_init(struct SupportModule *self, int width, int height, const char *home)
{
    MODULE_DEBUG_PRINTF("Module: Init(%i,%i,%s)\n",width,height);

    self->priv->JNI_OnLoad(VM_M, NULL);

    //self->priv->myHome = strdup(home);
    global = GLOBAL_M;
    self->priv->native_init(ENV_M, GLOBAL_M, width, height);
}

static void
badpiggies_input(struct SupportModule *self, int event, int x, int y, int finger)
{
    self->priv->native_input(ENV_M, GLOBAL_M, event, x, y, finger, 0, 0);
}

static void
badpiggies_update(struct SupportModule *self)
{
    self->priv->native_update(ENV_M, GLOBAL_M);
}

static void
jni_init(struct SupportModule *self)
{
    self->priv->initJni(ENV_M, GLOBAL_M);
}

static void
badpiggies_deinit(struct SupportModule *self)
{
    self->priv->native_deinit(ENV_M, GLOBAL_M);
    SDL_CloseAudio();
}

static void
badpiggies_pause(struct SupportModule *self)
{
    self->priv->native_pause(ENV_M, GLOBAL_M);
}

static void
badpiggies_resume(struct SupportModule *self)
{
    self->priv->native_resume(ENV_M, GLOBAL_M);
}

APKENV_MODULE(badpiggies, MODULE_PRIORITY_GAME)

