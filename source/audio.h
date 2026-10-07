#ifndef __AUDIO_H__
#define __AUDIO_H__

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_term(void);

int audio_load_sample(const char *name);
int audio_play_sample(int sample_id, float volume);
void audio_stop_sample(int stream_id);

void audio_play_music(const char *name);
void audio_stop_music(void);
void audio_pause_music(void);
void audio_resume_music(void);
void audio_set_music_volume(float volume);

#ifdef __cplusplus
}
#endif

#endif // __AUDIO_H__
