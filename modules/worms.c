
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
 * Worms support module 0.1
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
typedef void (*worms_init_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*worms_input_t)(JNIEnv *env, jobject obj, jint action, jfloat x, jfloat y, jint finger) SOFTFP;
typedef jboolean (*worms_update_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*worms_pause_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*worms_resume_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*worms_deinit_t)(JNIEnv *env, jobject obj) SOFTFP;
/*typedef jint (*worms_config_get_t)(JNIEnv *env, jobject obj, jstring paramString, jint paramInt) SOFTFP;
typedef jint (*worms_config_get_int_t)(JNIEnv *env, jobject obj, jstring paramString1, jstring paramString2, jint *paramArrayOfInt) SOFTFP;
typedef jint (*worms_config_get_string_t)(JNIEnv *env, jobject obj, jstring paramString1, jstring paramString2, jstring *paramArrayOfString) SOFTFP;
typedef void (*worms_debug_trace_line_t)(JNIEnv *env, jobject obj, jstring paramString) SOFTFP;
typedef void (*worms_device_yield_t)(JNIEnv *env, jobject obj, jint paramInt) SOFTFP;*/


struct SupportModulePriv {
    jni_onload_t JNI_OnLoad;
    jni_onunload_t JNI_OnUnLoad;
    worms_init_t native_init;
    worms_update_t native_update;
    worms_input_t native_input;
    worms_pause_t native_pause;
    worms_resume_t native_resume;
    worms_deinit_t native_deinit;
    /*worms_config_get_t native_config;
    worms_config_get_int_t native_config_int;
    worms_config_get_string_t native_config_string;
    worms_debug_trace_line_t native_trace_line;
    worms_device_yield_t native_device_yield;*/
    const char *myHome;
};
static struct SupportModulePriv worms_priv;

/* Audio specs and handle */
SDL_AudioSpec *desired, *obtained;
jlong audioHandle;

/* Global application state so we can call this from override thingie */
struct GlobalState *global;
char *worms_libname;

/* Fill audio buffer */
void my_audio_callback(void *ud, Uint8 *stream, int len)
{
    //worms_priv.native_mixdata(ENV(global), VM(global), audioHandle, stream, len);
}


/* CallVoidMethodV override. Signal when to start or stop audio */
void
JNIEnv_CallVoidMethodV(JNIEnv* p0, jobject p1, jmethodID p2, va_list p3)
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
jobject
JNIEnv_NewObjectV(JNIEnv *env, jclass p1, jmethodID p2, va_list p3)
{
    struct dummy_jclass *clazz = p1;
    MODULE_DEBUG_PRINTF("module_JNIEnv_NewObjectV(%x, %s, %s)\n", p1, p2->name, clazz->name);
    if (strcmp(clazz->name, "com/ideaworks3d/airplay/SoundPlayer") == 0)
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
jobject
JNIEnv_CallObjectMethodV(JNIEnv *env, jobject p1, jmethodID p2, va_list p3)
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
void
JNIEnv_DeleteLocalRef(JNIEnv* p0, jobject p1)
{
    MODULE_DEBUG_PRINTF("JNIEnv_DeleteLocalRef(%x)\n", p1);
    if (p1 == GLOBAL_J(p0) || p1 == NULL) {
        MODULE_DEBUG_PRINTF("WARNING: DeleteLocalRef on global\n");
        return;
    }
    free(p1);
}

static int
worms_try_init(struct SupportModule *self)
{
    printf("Trying %s\n", LOOKUP_M("Java_com_ideaworks3d_airplay_AirplayThread_initNative"));

    self->priv->JNI_OnLoad = (jni_onload_t)LOOKUP_M("JNI_OnLoad");
    self->priv->JNI_OnUnLoad = (jni_onunload_t)LOOKUP_M("JNI_OnUnLoad");
    self->priv->native_init = (worms_init_t)LOOKUP_M("airplay_AirplayThread_initNative");
    self->priv->native_input = (worms_input_t)LOOKUP_M("onMotionEvent");
    self->priv->native_update = (worms_update_t)LOOKUP_M("runOnOSTickNative");
    self->priv->native_pause = (worms_pause_t)LOOKUP_M("onPauseNative");
    self->priv->native_resume = (worms_resume_t)LOOKUP_M("resumeAppThreads");
    self->priv->native_deinit = (worms_deinit_t)LOOKUP_M("shutdownNative");

    printf("OnLoad: %s\n", self->priv->JNI_OnLoad ? "true" : "false");
    printf("OnUnLoad: %s\n", self->priv->JNI_OnUnLoad ? "true" : "false");
    printf("init: %s\n", self->priv->native_init ? "true" : "false");
    printf("input: %s\n", self->priv->native_input ? "true" : "false");
    printf("update: %s\n", self->priv->native_update ? "true" : "false");
    printf("pause: %s\n", self->priv->native_pause ? "true" : "false");
    printf("resume: %s\n", self->priv->native_resume ? "true" : "false");
    printf("deinit: %s\n", self->priv->native_deinit ? "true" : "false");

    // self->priv->native_config = (worms_config_get_t)LOOKUP_M("airplay_AirplayAPI_s3eConfigGet");
    // self->priv->native_config_int = (worms_config_get_int_t)LOOKUP_M("airplay_AirplayAPI_s3eConfigGetInt");
    // self->priv->native_config_string = (worms_config_get_string_t)LOOKUP_M("airplay_AirplayAPI_s3eConfigGetString");
    // self->priv->native_trace_line = (worms_debug_trace_line_t)LOOKUP_M("airplay_AirplayAPI_s3eDebugTraceLine");
    // self->priv->native_device_yield = (worms_device_yield_t)LOOKUP_M("airplay_AirplayAPI_s3eDeviceYield");

    /* Overrides for JNIEnv_ */
    self->override_env.CallObjectMethodV = JNIEnv_CallObjectMethodV;
    self->override_env.DeleteLocalRef = JNIEnv_DeleteLocalRef;
    self->override_env.CallVoidMethodV = JNIEnv_CallVoidMethodV;
    self->override_env.NewObjectV = JNIEnv_NewObjectV;

    return (self->priv->JNI_OnLoad != NULL &&
            self->priv->JNI_OnUnLoad != NULL /*&&
            self->priv->native_init != NULL &&
            self->priv->native_input != NULL &&
            self->priv->native_update != NULL &&
            self->priv->native_pause != NULL &&
            self->priv->native_resume != NULL &&
            self->priv->native_deinit != NULL /*&&
            self->priv->native_config != NULL &&
            self->priv->native_config_int != NULL &&
            self->priv->native_config_string != NULL &&
            self->priv->native_trace_line != NULL &&
            self->priv->native_device_yield != NULL*/);
}

static void
worms_init(struct SupportModule *self, int width, int height, const char *home)
{
    MODULE_DEBUG_PRINTF("Module: Init(%i,%i,%s)\n",width,height,home);

    self->priv->myHome = strdup(home);
    global = GLOBAL_M;

    self->priv->JNI_OnLoad(VM_M, NULL);

    self->priv->native_init(ENV_M, GLOBAL_M);
}

static void
worms_input(struct SupportModule *self, int event, int x, int y, int finger)
{
    self->priv->native_input(ENV_M, GLOBAL_M, event, x, y, finger);
}

static void
worms_update(struct SupportModule *self)
{
    self->priv->native_update(ENV_M, GLOBAL_M);
}

static void
worms_deinit(struct SupportModule *self)
{
    self->priv->native_deinit(ENV_M, GLOBAL_M);

    self->priv->JNI_OnUnLoad(VM_M, NULL);

    SDL_CloseAudio();
}

static void
worms_pause(struct SupportModule *self)
{
    self->priv->native_pause(ENV_M, GLOBAL_M);
}

static void
worms_resume(struct SupportModule *self)
{
    self->priv->native_resume(ENV_M, GLOBAL_M);
}

/*static int
worms_config_get(struct SupportModule *self, char *paramString, int paramInt)
{
    self->priv->native_config(ENV_M, GLOBAL_M, paramString, paramInt);
}

static int
worms_config_get_int(struct SupportModule *self, char *paramString1, char *paramString2, int *paramArrayOfInt)
{
    self->priv->native_config_int(ENV_M, GLOBAL_M, paramString1, paramString2, paramArrayOfInt);
}

static int
worms_config_get_string(struct SupportModule *self, char *paramString1, char *paramString2, void **paramArrayOfString)
{
    self->priv->native_config_string(ENV_M, GLOBAL_M, paramString1, paramString2, paramArrayOfString);
}

static void
worms_debug_trace_line(struct SupportModule *self, char *paramString)
{
    self->priv->native_trace_line(ENV_M, GLOBAL_M, paramString);
}

static void
worms_device_yield(struct SupportModule *self, int paramInt)
{
    self->priv->native_device_yield(ENV_M, GLOBAL_M, paramInt);
}*/

APKENV_MODULE(worms, MODULE_PRIORITY_GAME)

