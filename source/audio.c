/**
 * @file audio.c
 * @brief Multi-voice Ogg Vorbis Audio Mixer for Snail Mail (PS Vita).
 */

#include "audio.h"
#include "utils/logger.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <tremor/ivorbisfile.h>

#define AUDIO_RATE 44100
#define AUDIO_GRAIN 1024

#define VOICE_BGM    0
#define VOICE_SFX0   1
#define NUM_VOICES   8

#define MAX_SAMPLES  256

typedef struct {
    OggVorbis_File vf;
    int active;
    int paused;
    int loop;
    int channels;
    long rate;
    float gain;
    float pos_frac;
    int16_t prev_l, prev_r;
    int have_prev;
    int16_t stage[256];
    int stage_frames, stage_pos;
} voice_t;

typedef struct {
    char name[64];
    char path[256];
} sample_entry_t;

static voice_t voices[NUM_VOICES];
static sample_entry_t samples[MAX_SAMPLES];
static int num_samples = 0;

static SceUID audio_mutex = -1;
static SceUID audio_thread_id = -1;
static int audio_port = -1;
static volatile int audio_running = 0;

static void voice_close(voice_t *v) {
    if (v->active) {
        ov_clear(&v->vf);
        v->active = 0;
        v->paused = 0;
    }
}

static int voice_next_src_frame(voice_t *v, int16_t *l, int16_t *r) {
    while (v->stage_pos >= v->stage_frames) {
        int bs;
        long got = ov_read(&v->vf, (char *) v->stage, sizeof(v->stage), &bs);
        if (got <= 0) {
            if (v->loop && got == 0 && ov_pcm_seek(&v->vf, 0) == 0)
                continue;
            voice_close(v);
            return 0;
        }
        v->stage_frames = (int) got / (v->channels * 2);
        v->stage_pos = 0;
    }

    *l = v->stage[v->stage_pos * v->channels];
    *r = v->stage[v->stage_pos * v->channels + (v->channels > 1 ? 1 : 0)];
    v->stage_pos++;
    return 1;
}

static int voice_decode(voice_t *v, int16_t *out, int frames) {
    int done = 0;
    if (!v->active || v->paused) return 0;

    if (v->rate == AUDIO_RATE) {
        int16_t l, r;
        while (done < frames && v->active && !v->paused) {
            if (!voice_next_src_frame(v, &l, &r)) break;
            out[done * 2]     = (int16_t)(l * v->gain);
            out[done * 2 + 1] = (int16_t)(r * v->gain);
            done++;
        }
        return done;
    }

    float step = (float) v->rate / (float) AUDIO_RATE;
    while (done < frames && v->active && !v->paused) {
        while (v->pos_frac >= 1.0f && v->active) {
            v->prev_l = v->stage[v->stage_pos * v->channels];
            v->prev_r = v->stage[v->stage_pos * v->channels + (v->channels > 1 ? 1 : 0)];
            v->have_prev = 1;
            int16_t dummy_l, dummy_r;
            if (!voice_next_src_frame(v, &dummy_l, &dummy_r)) break;
            v->pos_frac -= 1.0f;
        }
        if (!v->active || v->paused) break;

        int16_t cur_l = v->stage[v->stage_pos * v->channels];
        int16_t cur_r = v->stage[v->stage_pos * v->channels + (v->channels > 1 ? 1 : 0)];
        float p0_l = v->have_prev ? (float) v->prev_l : (float) cur_l;
        float p0_r = v->have_prev ? (float) v->prev_r : (float) cur_r;
        float frac = v->pos_frac;

        float samp_l = (p0_l + frac * ((float) cur_l - p0_l)) * v->gain;
        float samp_r = (p0_r + frac * ((float) cur_r - p0_r)) * v->gain;

        if (samp_l > 32767.0f) samp_l = 32767.0f;
        else if (samp_l < -32768.0f) samp_l = -32768.0f;
        if (samp_r > 32767.0f) samp_r = 32767.0f;
        else if (samp_r < -32768.0f) samp_r = -32768.0f;

        out[done * 2]     = (int16_t) samp_l;
        out[done * 2 + 1] = (int16_t) samp_r;
        done++;
        v->pos_frac += step;
    }
    return done;
}

static int audio_thread(SceSize args, void *argp) {
    int32_t mix_buf[AUDIO_GRAIN * 2];
    int16_t out_buf[AUDIO_GRAIN * 2];
    int16_t voice_buf[AUDIO_GRAIN * 2];

    while (audio_running) {
        memset(mix_buf, 0, sizeof(mix_buf));

        sceKernelLockMutex(audio_mutex, 1, NULL);
        for (int i = 0; i < NUM_VOICES; i++) {
            if (!voices[i].active || voices[i].paused) continue;
            int got = voice_decode(&voices[i], voice_buf, AUDIO_GRAIN);
            for (int k = 0; k < got * 2; k++) {
                mix_buf[k] += voice_buf[k];
            }
        }
        sceKernelUnlockMutex(audio_mutex, 1);

        for (int k = 0; k < AUDIO_GRAIN * 2; k++) {
            int32_t v = mix_buf[k];
            if (v > 32767) v = 32767;
            else if (v < -32768) v = -32768;
            out_buf[k] = (int16_t) v;
        }

        sceAudioOutOutput(audio_port, out_buf);
    }
    return 0;
}

void audio_init(void) {
    if (audio_running) return;

    memset(voices, 0, sizeof(voices));
    memset(samples, 0, sizeof(samples));
    num_samples = 0;

    audio_mutex = sceKernelCreateMutex("snailmail_audio_mtx", 0, 0, NULL);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_GRAIN, AUDIO_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    if (audio_port < 0) {
        l_error("[Audio] Failed to open audio port: 0x%08x", audio_port);
        return;
    }

    int vol[2] = { SCE_AUDIO_VOLUME_0DB, SCE_AUDIO_VOLUME_0DB };
    sceAudioOutSetVolume(audio_port, SCE_AUDIO_VOLUME_FLAG_L_CH | SCE_AUDIO_VOLUME_FLAG_R_CH, vol);

    audio_running = 1;
    audio_thread_id = sceKernelCreateThread("snailmail_audio_thread", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    if (audio_thread_id >= 0) {
        sceKernelChangeThreadCpuAffinityMask(audio_thread_id, SCE_KERNEL_CPU_MASK_USER_1);
        sceKernelStartThread(audio_thread_id, 0, NULL);
        l_success("[Audio] Audio mixer initialized.");
    }
}

void audio_term(void) {
    if (!audio_running) return;
    audio_running = 0;

    if (audio_thread_id >= 0) {
        sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL);
        sceKernelDeleteThread(audio_thread_id);
        audio_thread_id = -1;
    }

    for (int i = 0; i < NUM_VOICES; i++) {
        voice_close(&voices[i]);
    }

    if (audio_port >= 0) {
        sceAudioOutReleasePort(audio_port);
        audio_port = -1;
    }


    if (audio_mutex >= 0) {
        sceKernelDeleteMutex(audio_mutex);
        audio_mutex = -1;
    }
}

static int resolve_audio_path(const char *name, char *out, size_t out_size) {
    if (!name || name[0] == '\0') return 0;

    // Check with .ogg suffix if not present
    const char *ext = strstr(name, ".ogg");
    if (ext) {
        snprintf(out, out_size, "%sassets/%s", DATA_PATH, name);
    } else {
        snprintf(out, out_size, "%sassets/%s.ogg", DATA_PATH, name);
    }

    if (access(out, F_OK) == 0) return 1;

    // Fallback directly under data
    if (ext) {
        snprintf(out, out_size, "%s%s", DATA_PATH, name);
    } else {
        snprintf(out, out_size, "%s%s.ogg", DATA_PATH, name);
    }
    if (access(out, F_OK) == 0) return 1;

    return 0;
}

int audio_load_sample(const char *name) {
    if (!name || name[0] == '\0') return -1;

    // Check if already registered
    for (int i = 0; i < num_samples; i++) {
        if (strcmp(samples[i].name, name) == 0) {
            return i + 1;
        }
    }

    if (num_samples >= MAX_SAMPLES) {
        l_warn("[Audio] Max samples reached, cannot register %s", name);
        return -1;
    }

    char path[256];
    if (!resolve_audio_path(name, path, sizeof(path))) {
        l_warn("[Audio] Sample file not found: %s", name);
        return -1;
    }

    int idx = num_samples++;
    strncpy(samples[idx].name, name, sizeof(samples[idx].name) - 1);
    strncpy(samples[idx].path, path, sizeof(samples[idx].path) - 1);

    return idx + 1; // 1-indexed sample id
}

int audio_play_sample(int sample_id, float volume) {
    if (!audio_running || sample_id < 1 || sample_id > num_samples) return 0;
    if (volume <= 0.0f) volume = 1.0f;

    const char *path = samples[sample_id - 1].path;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;

    OggVorbis_File vf;
    if (ov_open(f, &vf, NULL, 0) < 0) {
        fclose(f);
        return 0;
    }

    vorbis_info *vi = ov_info(&vf, -1);
    if (!vi || (vi->channels != 1 && vi->channels != 2)) {
        ov_clear(&vf);
        return 0;
    }

    static int next_sfx = 0;
    int voice_idx = VOICE_SFX0 + (next_sfx++ % (NUM_VOICES - VOICE_SFX0));

    sceKernelLockMutex(audio_mutex, 1, NULL);
    voice_close(&voices[voice_idx]);
    voices[voice_idx].vf = vf;
    voices[voice_idx].active = 1;
    voices[voice_idx].paused = 0;
    voices[voice_idx].loop = 0;
    voices[voice_idx].channels = vi->channels;
    voices[voice_idx].rate = vi->rate;
    voices[voice_idx].gain = volume;
    voices[voice_idx].pos_frac = 0.0f;
    voices[voice_idx].have_prev = 0;
    voices[voice_idx].stage_frames = 0;
    voices[voice_idx].stage_pos = 0;
    sceKernelUnlockMutex(audio_mutex, 1);

    return voice_idx + 1;
}

void audio_stop_sample(int stream_id) {
    if (!audio_running || stream_id < 1 || stream_id > NUM_VOICES) return;
    int voice_idx = stream_id - 1;

    sceKernelLockMutex(audio_mutex, 1, NULL);
    voice_close(&voices[voice_idx]);
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_play_music(const char *name) {
    if (!audio_running || !name || name[0] == '\0') return;

    char path[256];
    if (!resolve_audio_path(name, path, sizeof(path))) {
        l_warn("[Audio] Music file not found: %s", name);
        return;
    }

    FILE *f = fopen(path, "rb");
    if (!f) return;

    OggVorbis_File vf;
    if (ov_open(f, &vf, NULL, 0) < 0) {
        fclose(f);
        l_warn("[Audio] Failed to open music stream: %s", path);
        return;
    }

    vorbis_info *vi = ov_info(&vf, -1);
    if (!vi || (vi->channels != 1 && vi->channels != 2)) {
        ov_clear(&vf);
        return;
    }

    sceKernelLockMutex(audio_mutex, 1, NULL);
    voice_close(&voices[VOICE_BGM]);
    voices[VOICE_BGM].vf = vf;
    voices[VOICE_BGM].active = 1;
    voices[VOICE_BGM].paused = 0;
    voices[VOICE_BGM].loop = 1;
    voices[VOICE_BGM].channels = vi->channels;
    voices[VOICE_BGM].rate = vi->rate;
    voices[VOICE_BGM].gain = 1.0f;
    voices[VOICE_BGM].pos_frac = 0.0f;
    voices[VOICE_BGM].have_prev = 0;
    voices[VOICE_BGM].stage_frames = 0;
    voices[VOICE_BGM].stage_pos = 0;
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_stop_music(void) {
    if (!audio_running) return;
    sceKernelLockMutex(audio_mutex, 1, NULL);
    voice_close(&voices[VOICE_BGM]);
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_pause_music(void) {
    if (!audio_running) return;
    sceKernelLockMutex(audio_mutex, 1, NULL);
    voices[VOICE_BGM].paused = 1;
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_resume_music(void) {
    if (!audio_running) return;
    sceKernelLockMutex(audio_mutex, 1, NULL);
    voices[VOICE_BGM].paused = 0;
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_set_music_volume(float volume) {
    if (!audio_running) return;
    if (volume < 0.0f) volume = 0.0f;
    if (volume > 1.0f) volume = 1.0f;

    sceKernelLockMutex(audio_mutex, 1, NULL);
    voices[VOICE_BGM].gain = volume;
    sceKernelUnlockMutex(audio_mutex, 1);
}
