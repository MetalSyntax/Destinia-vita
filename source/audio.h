/*
 * audio.h
 *
 * Audio engine for DESTINIA PS Vita port.
 * Uses Tremor (libvorbisidec) and SceAudioOut with a dedicated mixing thread.
 */

#ifndef DESTINIA_AUDIO_H
#define DESTINIA_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_play_sound(int sound_id);
void audio_set_volume(int sound_type, int cur, int max);
void audio_stop_bgm(void);
void audio_stop_all(void);
void audio_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // DESTINIA_AUDIO_H
