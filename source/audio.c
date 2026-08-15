/*
 * audio.c
 *
 * Audio engine for DESTINIA PS Vita port.
 * Mixed output via SceAudioOut and Tremor (libvorbisidec).
 */

#include "audio.h"

#include <psp2/audioout.h>
#include <psp2/kernel/threadmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <tremor/ivorbisfile.h>
#include "utils/logger.h"

#define AUDIO_RATE 44100
#define AUDIO_GRAIN 512

#define VOICE_BGM  0
#define VOICE_SFX0 1
#define NUM_VOICES 5

typedef struct {
    OggVorbis_File vf;
    int active;
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

static voice_t voices[NUM_VOICES];
static SceUID audio_mutex = -1;
static SceUID audio_thread_id = -1;
static int audio_port = -1;
static volatile int audio_running = 0;

static float bgm_volume = 1.0f;
static float sfx_volume = 1.0f;
static int cur_bgm_id = -1;
static int next_sfx_voice = VOICE_SFX0;

static int audio_resolve_path(int snd_id, char *out, size_t out_size) {
    char base_name[64];
    if (snd_id >= 100) {
        snprintf(base_name, sizeof(base_name), "bg%03d.ogg", snd_id);
    } else if (snd_id == 50) {
        snprintf(base_name, sizeof(base_name), "eff50.ogg");
    } else {
        snprintf(base_name, sizeof(base_name), "eff%02d.ogg", snd_id % 100);
    }

    snprintf(out, out_size, "ux0:data/destinia/sound/%s", base_name);
    if (access(out, F_OK) == 0) return 1;

    snprintf(out, out_size, "ux0:data/destinia/assets/%s", base_name);
    if (access(out, F_OK) == 0) return 1;

    snprintf(out, out_size, "ux0:data/destinia/assets/sound/%s", base_name);
    if (access(out, F_OK) == 0) return 1;

    snprintf(out, out_size, "ux0:data/destinia/%s", base_name);
    if (access(out, F_OK) == 0) return 1;

    return 0;
}

static void voice_close(voice_t *v) {
    if (v->active) {
        ov_clear(&v->vf);
        v->active = 0;
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

    if (v->rate == AUDIO_RATE) {
        int16_t l, r;
        while (done < frames && v->active) {
            if (!voice_next_src_frame(v, &l, &r)) break;
            out[done * 2]     = (int16_t)(l * v->gain);
            out[done * 2 + 1] = (int16_t)(r * v->gain);
            done++;
        }
        return done;
    }

    float step = (float) v->rate / (float) AUDIO_RATE;
    while (done < frames && v->active) {
        if (!v->have_prev) {
            if (!voice_next_src_frame(v, &v->prev_l, &v->prev_r)) break;
            v->have_prev = 1;
            v->pos_frac = 0.0f;
        }
        int16_t nl = v->prev_l, nr = v->prev_r;
        while (v->pos_frac >= 1.0f) {
            if (!voice_next_src_frame(v, &nl, &nr)) { v->have_prev = 0; break; }
            v->prev_l = nl; v->prev_r = nr;
            v->pos_frac -= 1.0f;
        }
        if (!v->active || !v->have_prev) break;

        out[done * 2]     = (int16_t)(v->prev_l * v->gain);
        out[done * 2 + 1] = (int16_t)(v->prev_r * v->gain);
        done++;
        v->pos_frac += step;
    }
    return done;
}

static int audio_thread(SceSize args, void *argp) {
    static int16_t mix[AUDIO_GRAIN * 2];
    static int16_t buf[AUDIO_GRAIN * 2];

    while (audio_running) {
        memset(mix, 0, sizeof(mix));

        sceKernelLockMutex(audio_mutex, 1, NULL);
        for (int vi = 0; vi < NUM_VOICES; vi++) {
            voice_t *v = &voices[vi];
            if (!v->active) continue;
            int got = voice_decode(v, buf, AUDIO_GRAIN);
            for (int i = 0; i < got * 2; i++) {
                int s = mix[i] + buf[i];
                if (s > 32767) s = 32767;
                if (s < -32768) s = -32768;
                mix[i] = (int16_t) s;
            }
        }
        sceKernelUnlockMutex(audio_mutex, 1);

        sceAudioOutOutput(audio_port, mix);
    }
    return 0;
}

void audio_init(void) {
    memset(voices, 0, sizeof(voices));
    audio_mutex = sceKernelCreateMutex("destinia_audio_mutex", 0, 0, NULL);
    audio_port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_BGM, AUDIO_GRAIN, AUDIO_RATE, SCE_AUDIO_OUT_MODE_STEREO);
    audio_running = 1;
    audio_thread_id = sceKernelCreateThread("destinia_audio_thread", audio_thread, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(audio_thread_id, 0, NULL);
    l_info("[Audio] Subsystem initialized successfully.");
}

void audio_play_sound(int sound_id) {
    if (sound_id < 0) return;

    char path[256];
    if (!audio_resolve_path(sound_id, path, sizeof(path))) {
        l_warn("[Audio] Sound file for ID %d not found.", sound_id);
        return;
    }

    FILE *f = fopen(path, "rb");
    if (!f) {
        l_warn("[Audio] Failed to open sound file: %s", path);
        return;
    }

    sceKernelLockMutex(audio_mutex, 1, NULL);

    int vi;
    int is_bgm = (sound_id >= 100);

    if (is_bgm) {
        if (cur_bgm_id == sound_id && voices[VOICE_BGM].active) {
            // Already playing this BGM
            fclose(f);
            sceKernelUnlockMutex(audio_mutex, 1);
            return;
        }
        vi = VOICE_BGM;
        cur_bgm_id = sound_id;
    } else {
        vi = next_sfx_voice;
        next_sfx_voice++;
        if (next_sfx_voice >= NUM_VOICES)
            next_sfx_voice = VOICE_SFX0;
    }

    voice_close(&voices[vi]);

    if (ov_open(f, &voices[vi].vf, NULL, 0) < 0) {
        l_warn("[Audio] ov_open failed for %s", path);
        fclose(f);
        sceKernelUnlockMutex(audio_mutex, 1);
        return;
    }

    vorbis_info *vi_info = ov_info(&voices[vi].vf, -1);
    voices[vi].channels = vi_info->channels;
    voices[vi].rate = vi_info->rate;
    voices[vi].loop = is_bgm ? 1 : 0;
    voices[vi].gain = is_bgm ? bgm_volume : sfx_volume;
    voices[vi].pos_frac = 0.0f;
    voices[vi].have_prev = 0;
    voices[vi].stage_frames = 0;
    voices[vi].stage_pos = 0;
    voices[vi].active = 1;

    l_debug("[Audio] Playing %s on voice %d (rate=%ld, ch=%d, loop=%d)", path, vi, voices[vi].rate, voices[vi].channels, voices[vi].loop);

    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_set_volume(int sound_type, int cur, int max) {
    if (max <= 0) return;
    float vol = (float)cur / (float)max;
    if (vol < 0.0f) vol = 0.0f;
    if (vol > 1.0f) vol = 1.0f;

    sceKernelLockMutex(audio_mutex, 1, NULL);
    if (sound_type == 0) { // BGM
        bgm_volume = vol;
        if (voices[VOICE_BGM].active) {
            voices[VOICE_BGM].gain = bgm_volume;
        }
    } else if (sound_type == 1) { // SFX
        sfx_volume = vol;
        for (int i = VOICE_SFX0; i < NUM_VOICES; i++) {
            if (voices[i].active) {
                voices[i].gain = sfx_volume;
            }
        }
    }
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_stop_bgm(void) {
    sceKernelLockMutex(audio_mutex, 1, NULL);
    voice_close(&voices[VOICE_BGM]);
    cur_bgm_id = -1;
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_stop_all(void) {
    sceKernelLockMutex(audio_mutex, 1, NULL);
    for (int i = 0; i < NUM_VOICES; i++) {
        voice_close(&voices[i]);
    }
    cur_bgm_id = -1;
    sceKernelUnlockMutex(audio_mutex, 1);
}

void audio_shutdown(void) {
    audio_running = 0;
    sceKernelWaitThreadEnd(audio_thread_id, NULL, NULL);
    sceKernelDeleteThread(audio_thread_id);
    sceKernelDeleteMutex(audio_mutex);
    sceAudioOutReleasePort(audio_port);
}
