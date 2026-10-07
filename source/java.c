#include <falso_jni/FalsoJNI_Impl.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <zlib.h>

#include "audio.h"
#include "utils/logger.h"
#include "stb/stb_image.h"

/*
 * Method IDs (1-based)
 */
enum {
    MID_JAVALoadSample = 1,
    MID_JAVAPlaySample,
    MID_JAVAStopSample,
    MID_JAVASaveFile,
    MID_JAVADeleteFile,
    MID_JAVALoadFile,
    MID_JAVAFindFile,
    MID_JAVAFileSize,
    MID_JAVASetMusicVolume,
    MID_JAVAPlayMusic,
    MID_JAVAStopMusic,
    MID_JAVAUnPauseMusic,
    MID_JAVAPauseMusic,
    MID_JAVAMusicRestart,
    MID_JAVAUnZip,
    MID_JAVAUnJpg,
    MID_JAVAUnPng,
    MID_JAVAOpenFeintOpen,
    MID_JAVAOpenFeintLastLoggedInUserID,
    MID_JAVAOpenFeintSubmit,
    MID_JAVAOpenFeintUnlock,
    MID_JAVAOpenFeintIsUserLoggedIn,
    MID_JAVAOpenFeintIsOnline,
    MID_JAVATime,
    MID_JAVATimeHi,
    MID_JAVAVibrate,
};

static uint64_t s_jtime = 0;

/*
 * Method implementations
 */

static jint jni_JAVALoadSample(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    const char *name = (const char *)name_obj;
    return (jint)audio_load_sample(name);
}

static jint jni_JAVAPlaySample(jmethodID id, va_list args) {
    jint sample_id = va_arg(args, jint);
    double vol = va_arg(args, double);
    return (jint)audio_play_sample((int)sample_id, (float)vol);
}

static void jni_JAVAStopSample(jmethodID id, va_list args) {
    jint stream_id = va_arg(args, jint);
    audio_stop_sample((int)stream_id);
}

static jint jni_JAVASaveFile(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    jbyteArray data_obj = va_arg(args, jbyteArray);
    jint len = va_arg(args, jint);

    const char *name = (const char *)name_obj;
    const void *data = (const void *)data_obj;

    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fwrite(data, 1, len, f);
    fclose(f);
    return 1;
}

static void jni_JAVADeleteFile(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    const char *name = (const char *)name_obj;
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    remove(path);
}

static void jni_JAVALoadFile(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    jbyteArray data_obj = va_arg(args, jbyteArray);
    jint len = va_arg(args, jint);

    const char *name = (const char *)name_obj;
    void *data = (void *)data_obj;

    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    FILE *f = fopen(path, "rb");
    if (f) {
        fread(data, 1, len, f);
        fclose(f);
    }
}

static jint jni_JAVAFindFile(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    const char *name = (const char *)name_obj;
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    return access(path, F_OK) == 0 ? 1 : 0;
}

static jint jni_JAVAFileSize(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    const char *name = (const char *)name_obj;
    char path[256];
    snprintf(path, sizeof(path), "%ssaves/%s", DATA_PATH, name);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    int sz = ftell(f);
    fclose(f);
    return sz;
}

static void jni_JAVASetMusicVolume(jmethodID id, va_list args) {
    double vol = va_arg(args, double);
    audio_set_music_volume((float)vol);
}

static void jni_JAVAPlayMusic(jmethodID id, va_list args) {
    jstring name_obj = va_arg(args, jstring);
    const char *name = (const char *)name_obj;
    audio_play_music(name);
}

static void jni_JAVAStopMusic(jmethodID id, va_list args) {
    audio_stop_music();
}

static void jni_JAVAUnPauseMusic(jmethodID id, va_list args) {
    audio_resume_music();
}

static void jni_JAVAPauseMusic(jmethodID id, va_list args) {
    audio_pause_music();
}

static void jni_JAVAMusicRestart(jmethodID id, va_list args) {
    audio_resume_music();
}

static void jni_JAVAUnZip(jmethodID id, va_list args) {
    void *dst = va_arg(args, void *);
    void *src = va_arg(args, void *);
    // In Java, ZipInputStream reads first entry into Buffer.
    // Lengths are typically determined by zip headers or pre-sized buffer.
}

static void jni_JAVAUnJpg(jmethodID id, va_list args) {
    void *dst = va_arg(args, void *);
    void *src = va_arg(args, void *);
}

static void jni_JAVAUnPng(jmethodID id, va_list args) {
    void *dst = va_arg(args, void *);
    void *src = va_arg(args, void *);
}

static void jni_JAVAOpenFeintOpen(jmethodID id, va_list args) {
}

static void jni_JAVAOpenFeintLastLoggedInUserID(jmethodID id, va_list args) {
    char *uid = va_arg(args, char *);
    if (uid) uid[0] = '\0';
}

static void jni_JAVAOpenFeintSubmit(jmethodID id, va_list args) {
    // Leadboard string, score, callback ptr
}

static void jni_JAVAOpenFeintUnlock(jmethodID id, va_list args) {
    // Achievement string, callback ptr
}

static jint jni_JAVAOpenFeintIsUserLoggedIn(jmethodID id, va_list args) {
    return 0;
}

static jint jni_JAVAOpenFeintIsOnline(jmethodID id, va_list args) {
    return 0;
}

static jint jni_JAVATime(jmethodID id, va_list args) {
    s_jtime = (uint64_t)sceKernelGetProcessTimeLow() * 1000ULL;
    return (jint)(s_jtime & 0xFFFFFFFF);
}

static jint jni_JAVATimeHi(jmethodID id, va_list args) {
    return (jint)(s_jtime >> 32);
}

static void jni_JAVAVibrate(jmethodID id, va_list args) {
}

NameToMethodID nameToMethodId[] = {
    { MID_JAVALoadSample, "JAVALoadSample" },
    { MID_JAVAPlaySample, "JAVAPlaySample" },
    { MID_JAVAStopSample, "JAVAStopSample" },
    { MID_JAVASaveFile, "JAVASaveFile" },
    { MID_JAVADeleteFile, "JAVADeleteFile" },
    { MID_JAVALoadFile, "JAVALoadFile" },
    { MID_JAVAFindFile, "JAVAFindFile" },
    { MID_JAVAFileSize, "JAVAFileSize" },
    { MID_JAVASetMusicVolume, "JAVASetMusicVolume" },
    { MID_JAVAPlayMusic, "JAVAPlayMusic" },
    { MID_JAVAStopMusic, "JAVAStopMusic" },
    { MID_JAVAUnPauseMusic, "JAVAUnPauseMusic" },
    { MID_JAVAPauseMusic, "JAVAPauseMusic" },
    { MID_JAVAMusicRestart, "JAVAMusicRestart" },
    { MID_JAVAUnZip, "JAVAUnZip" },
    { MID_JAVAUnJpg, "JAVAUnJpg" },
    { MID_JAVAUnPng, "JAVAUnPng" },
    { MID_JAVAOpenFeintOpen, "JAVAOpenFeintOpen" },
    { MID_JAVAOpenFeintLastLoggedInUserID, "JAVAOpenFeintLastLoggedInUserID" },
    { MID_JAVAOpenFeintSubmit, "JAVAOpenFeintSubmit" },
    { MID_JAVAOpenFeintUnlock, "JAVAOpenFeintUnlock" },
    { MID_JAVAOpenFeintIsUserLoggedIn, "JAVAOpenFeintIsUserLoggedIn" },
    { MID_JAVAOpenFeintIsOnline, "JAVAOpenFeintIsOnline" },
    { MID_JAVATime, "JAVATime" },
    { MID_JAVATimeHi, "JAVATimeHi" },
    { MID_JAVAVibrate, "JAVAVibrate" },
};

MethodsInt methodsInt[] = {
    { MID_JAVALoadSample, jni_JAVALoadSample },
    { MID_JAVAPlaySample, jni_JAVAPlaySample },
    { MID_JAVASaveFile, jni_JAVASaveFile },
    { MID_JAVAFindFile, jni_JAVAFindFile },
    { MID_JAVAFileSize, jni_JAVAFileSize },
    { MID_JAVAOpenFeintIsUserLoggedIn, jni_JAVAOpenFeintIsUserLoggedIn },
    { MID_JAVAOpenFeintIsOnline, jni_JAVAOpenFeintIsOnline },
    { MID_JAVATime, jni_JAVATime },
    { MID_JAVATimeHi, jni_JAVATimeHi },
};

MethodsVoid methodsVoid[] = {
    { MID_JAVAStopSample, jni_JAVAStopSample },
    { MID_JAVADeleteFile, jni_JAVADeleteFile },
    { MID_JAVALoadFile, jni_JAVALoadFile },
    { MID_JAVASetMusicVolume, jni_JAVASetMusicVolume },
    { MID_JAVAPlayMusic, jni_JAVAPlayMusic },
    { MID_JAVAStopMusic, jni_JAVAStopMusic },
    { MID_JAVAUnPauseMusic, jni_JAVAUnPauseMusic },
    { MID_JAVAPauseMusic, jni_JAVAPauseMusic },
    { MID_JAVAMusicRestart, jni_JAVAMusicRestart },
    { MID_JAVAUnZip, jni_JAVAUnZip },
    { MID_JAVAUnJpg, jni_JAVAUnJpg },
    { MID_JAVAUnPng, jni_JAVAUnPng },
    { MID_JAVAOpenFeintOpen, jni_JAVAOpenFeintOpen },
    { MID_JAVAOpenFeintLastLoggedInUserID, jni_JAVAOpenFeintLastLoggedInUserID },
    { MID_JAVAOpenFeintSubmit, jni_JAVAOpenFeintSubmit },
    { MID_JAVAOpenFeintUnlock, jni_JAVAOpenFeintUnlock },
    { MID_JAVAVibrate, jni_JAVAVibrate },
};

MethodsBoolean methodsBoolean[] = {};
MethodsByte methodsByte[] = {};
MethodsChar methodsChar[] = {};
MethodsDouble methodsDouble[] = {};
MethodsFloat methodsFloat[] = {};
MethodsLong methodsLong[] = {};
MethodsObject methodsObject[] = {};
MethodsShort methodsShort[] = {};

/*
 * JNI Fields
 */
char WINDOW_SERVICE[] = "window";
const int SDK_INT = 19;
static int s_dummy_fd = 0;

NameToFieldID nameToFieldId[] = {
    { 0, "WINDOW_SERVICE", FIELD_TYPE_OBJECT }, 
    { 1, "SDK_INT", FIELD_TYPE_INT },
    { 2, "descriptor", FIELD_TYPE_INT },
};

FieldsBoolean fieldsBoolean[] = {};
FieldsByte fieldsByte[] = {};
FieldsChar fieldsChar[] = {};
FieldsDouble fieldsDouble[] = {};
FieldsFloat fieldsFloat[] = {};
FieldsInt fieldsInt[] = {
    { 1, SDK_INT },
    { 2, 0 },
};
FieldsObject fieldsObject[] = {
    { 0, WINDOW_SERVICE },
};
FieldsLong fieldsLong[] = {};
FieldsShort fieldsShort[] = {};

__FALSOJNI_IMPL_CONTAINER_SIZES
