
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
 * Where's my Perry support module 0.1
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

typedef void (*perry_init_t)(JNIEnv *env, jobject obj, jstring paramString1, jstring paramString2, void *paramContext, jstring paramString3) SOFTFP;
typedef void (*perry_input_t)(JNIEnv *env, jobject obj, jint action, jfloat *x, jfloat *y, jint *finger) SOFTFP;
typedef jboolean (*perry_update_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*perry_pause_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*perry_resume_t)(JNIEnv *env, jobject obj) SOFTFP;
typedef void (*perry_resize_t)(JNIEnv *env, jobject obj, jint paramInt1, jint paramInt2) SOFTFP;
typedef void (*perry_deinit_t)(JNIEnv *env, jobject obj) SOFTFP;

struct SupportModulePriv {
    jni_onload_t JNI_OnLoad;
    jni_onunload_t JNI_OnUnLoad;
    perry_init_t native_init;
    perry_update_t native_update;
    perry_input_t native_input;
    perry_pause_t native_pause;
    perry_resume_t native_resume;
    perry_resize_t native_resize;
    perry_deinit_t native_deinit;
    const char *myHome;
};
static struct SupportModulePriv perry_priv;

/* Audio specs and handle */
SDL_AudioSpec *desired, *obtained;
jlong audioHandle;

/* Global application state so we can call this from override thingie */
struct GlobalState *global;

char *perry_libname = "libwmw.so";

/* Fill audio buffer */
void my_audio_callback(void *ud, Uint8 *stream, int len)
{
    //perry_priv.native_mixdata(ENV(global), VM(global), audioHandle, stream, len);
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
    if (strcmp(p2->name, "getInstallationId") == 0) {
        return (*env)->NewStringUTF(env, "");
    } else if (strcmp(p2->name, "getLanguageCode") == 0) {
        return (*env)->NewStringUTF(env, "en");
    } else if (strcmp(p2->name, "playSound") == 0) {
        // TODO: Play sound (doh!)
    } else {
        printf("Do not know what to do: %s\n", p2->name);
        exit(1);
    }
    return GLOBAL_J(env);
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
perry_try_init(struct SupportModule *self)
{
    printf("Trying %s\n", LOOKUP_M("rendererInit"));

    self->priv->JNI_OnLoad = (jni_onload_t)LOOKUP_M("JNI_OnLoad");
    self->priv->native_init = (perry_init_t)LOOKUP_M("rendererInit");
    self->priv->native_input = (perry_input_t)LOOKUP_M("rendererTouchEnded");
    self->priv->native_update = (perry_update_t)LOOKUP_M("rendererDrawFrame");
    self->priv->native_pause = (perry_pause_t)LOOKUP_M("onGamePause");
    self->priv->native_resume = (perry_resume_t)LOOKUP_M("onGameResume");
    self->priv->native_resize = (perry_resize_t)LOOKUP_M("rendererResized");
    //self->priv->native_deinit = (perry_deinit_t)LOOKUP_M("shutdownNative");
    
    printf("OnLoad: %s\n", self->priv->JNI_OnLoad ? "true" : "false");
    printf("init: %s\n", self->priv->native_init ? "true" : "false");
    printf("input: %s\n", self->priv->native_input ? "true" : "false");
    printf("update: %s\n", self->priv->native_update ? "true" : "false");
    printf("pause: %s\n", self->priv->native_pause ? "true" : "false");
    printf("resume: %s\n", self->priv->native_resume ? "true" : "false");
    printf("resize: %s\n", self->priv->native_resize ? "true" : "false");
    //printf("deinit: %s\n", self->priv->native_deinit ? "true" : "false");

    /* Overrides for JNIEnv_ */
    self->override_env.CallObjectMethodV = JNIEnv_CallObjectMethodV;
    self->override_env.DeleteLocalRef = JNIEnv_DeleteLocalRef;
    self->override_env.CallVoidMethodV = JNIEnv_CallVoidMethodV;
    self->override_env.NewObjectV = JNIEnv_NewObjectV;

    return (self->priv->JNI_OnLoad != NULL &&
            self->priv->native_init != NULL &&
            self->priv->native_input != NULL &&
            self->priv->native_update != NULL &&
            self->priv->native_pause != NULL &&
            self->priv->native_resize != NULL &&
            self->priv->native_resume != NULL /*&&
            self->priv->native_deinit != NULL*/);
}

static void
perry_init(struct SupportModule *self, int width, int height, const char *home)
{
    MODULE_DEBUG_PRINTF("Module: Init(%i,%i,%s)\n",width,height,home);

    self->priv->myHome = strdup(home);
    global = GLOBAL_M;

    self->priv->JNI_OnLoad(VM_M, NULL);

    self->priv->native_init(ENV_M, GLOBAL_M, GLOBAL_M->env->NewStringUTF(ENV_M, global->apk_filename), 
                                             GLOBAL_M->env->NewStringUTF(ENV_M, self->priv->myHome), 
                                             0, 
                                             "");
}

static void
perry_input(struct SupportModule *self, int event, int x, int y, int finger)
{
    self->priv->native_input(ENV_M, GLOBAL_M, event, (float*) x, (float*) y, (int*) finger);
}

static void
perry_update(struct SupportModule *self)
{
    self->priv->native_update(ENV_M, GLOBAL_M);
}

static void
perry_deinit(struct SupportModule *self)
{
    self->priv->native_deinit(ENV_M, GLOBAL_M);

    self->priv->JNI_OnUnLoad(VM_M, NULL);

    SDL_CloseAudio();
}

static void
perry_pause(struct SupportModule *self)
{
    self->priv->native_pause(ENV_M, GLOBAL_M);
}

static void
perry_resume(struct SupportModule *self)
{
    self->priv->native_resume(ENV_M, GLOBAL_M);
}

static void
perry_resize(struct SupportModule *self, int paramInt1, int paramInt2)
{
    self->priv->native_resize(ENV_M, GLOBAL_M, paramInt1, paramInt2);
}

APKENV_MODULE(perry, MODULE_PRIORITY_GAME)

