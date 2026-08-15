/*
 * main.c
 *
 * PS Vita native loader & wrapper for DESTINIA (Gamevil).
 */

#include "utils/init.h"
#include "utils/glutil.h"
#include "utils/logger.h"
#include "utils/utils.h"
#include "reimpl/io.h"
#include "java.h"
#include "audio.h"

#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/processmgr.h>
#include <psp2/ctrl.h>
#include <psp2/touch.h>
#include <psp2/power.h>

#include <falso_jni/FalsoJNI.h>
#include <so_util/so_util.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int _newlib_heap_size_user = 256 * 1024 * 1024;

#ifdef USE_SCELIBC_IO
int sceLibcHeapSize = 4 * 1024 * 1024;
#endif

so_module so_mod;

typedef void (*initGame_t)(JNIEnv *env, jobject thiz, jbyteArray uniqueID, jint ulen, jbyteArray model, jint mlen, jint w, jint h, jbyte isKorean);
typedef jint (*jniRun_t)(JNIEnv *env, jobject thiz, jbyteArray buffer, jint frame_count);
typedef void (*jniTouch_t)(JNIEnv *env, jobject thiz, jint x, jint y, jint action);
typedef void (*destroyGame_t)(JNIEnv *env, jobject thiz);
typedef jint (*bannerShow_t)(JNIEnv *env, jobject thiz);
typedef void (*setGamePlaying_t)(JNIEnv *env, jobject thiz, jbyte playing);
typedef void (*setEarnedCoin_t)(JNIEnv *env, jobject thiz, jint coin);

static initGame_t initGame_fn = NULL;
static jniRun_t jniRun_fn = NULL;
static jniTouch_t jniTouch_fn = NULL;
static destroyGame_t destroyGame_fn = NULL;
static bannerShow_t bannerShow_fn = NULL;

#define GAME_W 400
#define GAME_H 240
#define VITA_SCREEN_W 960.0f
#define VITA_SCREEN_H 544.0f

// Aspect ratio preserved scaling:
// 400x240 (5:3 aspect ratio = 1.6667)
// Render height = 544, Render width = 544 * (5/3) = 906.6667f
// Horizontal margins = (960 - 906.6667) / 2 = 26.6667f
#define RENDER_W 906.6667f
#define RENDER_H 544.0f
#define MARGIN_X 26.6667f

// Virtual button coordinates in 400x240 space
#define DPAD_CENTER_X 60
#define DPAD_CENTER_Y 175
#define DPAD_RADIUS   40

#define BTN_ATTACK_X  345
#define BTN_ATTACK_Y  185

#define BTN_SKILL1_X  300
#define BTN_SKILL1_Y  150

#define BTN_SKILL2_X  340
#define BTN_SKILL2_Y  125

#define BTN_SKILL3_X  380
#define BTN_SKILL3_Y  140

#define BTN_SLOT1_X   180
#define BTN_SLOT1_Y   215

#define BTN_SLOT2_X   220
#define BTN_SLOT2_Y   215

#define BTN_MAP_X     380
#define BTN_MAP_Y     20

int main(int argc, char *argv[]) {
    ensure_destinia_dirs();

    l_info("[Destinia] Starting loader initialization...");
    soloader_init_all();

    // Lookup exported native entry points
    initGame_fn = (initGame_t)so_symbol(&so_mod, "Java_game_destiniaeng_GameThread_initGame");
    jniRun_fn = (jniRun_t)so_symbol(&so_mod, "Java_game_destiniaeng_GameThread_jniRun");
    jniTouch_fn = (jniTouch_t)so_symbol(&so_mod, "Java_game_destiniaeng_GameThread_jniTouch");
    destroyGame_fn = (destroyGame_t)so_symbol(&so_mod, "Java_game_destiniaeng_GameThread_destroyGame");
    bannerShow_fn = (bannerShow_t)so_symbol(&so_mod, "Java_game_destiniaeng_GameThread_bannerShow");

    if (!initGame_fn || !jniRun_fn || !jniTouch_fn) {
        l_fatal("[Destinia] Failed to resolve required JNI entrypoints from libdestinia_jni.so!");
        return -1;
    }
    l_info("[Destinia] JNI symbols resolved successfully.");

    audio_init();
    gl_init();

    // Input initialization
    sceCtrlSetSamplingMode(SCE_CTRL_MODE_ANALOG);
    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);

    // Prepare JNI arguments
    const char *uid_str = "PSVITA0000000001";
    const char *model_str = "PlayStation Vita";

    jbyteArray j_uid = jni->NewByteArray(&jni, (jsize)strlen(uid_str));
    jbyte *uid_ptr = jni->GetByteArrayElements(&jni, j_uid, NULL);
    memcpy(uid_ptr, uid_str, strlen(uid_str));
    jni->ReleaseByteArrayElements(&jni, j_uid, uid_ptr, 0);

    jbyteArray j_model = jni->NewByteArray(&jni, (jsize)strlen(model_str));
    jbyte *model_ptr = jni->GetByteArrayElements(&jni, j_model, NULL);
    memcpy(model_ptr, model_str, strlen(model_str));
    jni->ReleaseByteArrayElements(&jni, j_model, model_ptr, 0);

    // 400 * 240 * 2 = 192000 bytes framebuffer
    jbyteArray j_buffer = jni->NewByteArray(&jni, 192000);
    uint16_t *framebuffer = (uint16_t *)jni->GetByteArrayElements(&jni, j_buffer, NULL);

    l_info("[Destinia] Calling initGame (width=%d, height=%d, isKorean=0)...", GAME_W, GAME_H);
    initGame_fn(&jni, (jobject)0x42424242, j_uid, (jint)strlen(uid_str), j_model, (jint)strlen(model_str), GAME_W, GAME_H, 0);
    l_info("[Destinia] Native initGame completed.");

    // OpenGL texture setup
    GLuint game_tex;
    glGenTextures(1, &game_tex);
    glBindTexture(GL_TEXTURE_2D, game_tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, GAME_W, GAME_H, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Quad geometry (Orthographic 2D)
    const GLfloat vertices[] = {
        MARGIN_X,            RENDER_H, 0.0f,
        MARGIN_X + RENDER_W, RENDER_H, 0.0f,
        MARGIN_X,            0.0f,     0.0f,
        MARGIN_X + RENDER_W, 0.0f,     0.0f,
    };

    const GLfloat texcoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };

    glViewport(0, 0, (GLsizei)VITA_SCREEN_W, (GLsizei)VITA_SCREEN_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f, VITA_SCREEN_W, VITA_SCREEN_H, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    int frame_count = 0;
    int prev_touch_active = 0;
    int prev_btn_active = 0;
    int prev_btn_x = -1, prev_btn_y = -1;
    uint32_t prev_buttons = 0;

    l_info("[Destinia] Entering main render and game loop...");

    while (1) {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(0, &pad, 1);

        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        // 1. Process Physical Start Button (Menu key)
        if ((pad.buttons & SCE_CTRL_START) && !(prev_buttons & SCE_CTRL_START)) {
            jniTouch_fn(&jni, (jobject)0x42424242, 0, 0, 3); // Action 3 = Menu
        }

        // 2. Process Touch Screen (Front Touch)
        int touch_active = (touch.reportNum > 0);
        if (touch_active) {
            float tx = (float)touch.report[0].x / 1920.0f * VITA_SCREEN_W;
            float ty = (float)touch.report[0].y / 1088.0f * VITA_SCREEN_H;

            // Map to 400x240 screen area
            float gx = (tx - MARGIN_X) * ((float)GAME_W / RENDER_W);
            float gy = ty * ((float)GAME_H / RENDER_H);

            if (gx < 0.0f) gx = 0.0f;
            if (gx >= (float)GAME_W) gx = (float)GAME_W - 1.0f;
            if (gy < 0.0f) gy = 0.0f;
            if (gy >= (float)GAME_H) gy = (float)GAME_H - 1.0f;

            int action = prev_touch_active ? 2 : 0; // 0=Down, 2=Move
            jniTouch_fn(&jni, (jobject)0x42424242, (jint)gx, (jint)gy, (jint)action);
            prev_touch_active = 1;
        } else if (prev_touch_active) {
            jniTouch_fn(&jni, (jobject)0x42424242, 0, 0, 1); // 1=Up
            prev_touch_active = 0;
        }

        // 3. Process Physical Buttons & Analog Sticks if no active front touch
        if (!touch_active) {
            int cur_btn_x = -1, cur_btn_y = -1;

            // Directional input: Analog stick or D-Pad
            int stick_dx = (int)pad.lx - 128;
            int stick_dy = (int)pad.ly - 128;
            int stick_mag = stick_dx * stick_dx + stick_dy * stick_dy;

            if (stick_mag > (35 * 35)) {
                float angle = atan2f((float)stick_dy, (float)stick_dx);
                cur_btn_x = DPAD_CENTER_X + (int)(cosf(angle) * (float)DPAD_RADIUS);
                cur_btn_y = DPAD_CENTER_Y + (int)(sinf(angle) * (float)DPAD_RADIUS);
            } else if ((pad.buttons & SCE_CTRL_UP) && (pad.buttons & SCE_CTRL_LEFT)) {
                cur_btn_x = DPAD_CENTER_X - 28; cur_btn_y = DPAD_CENTER_Y - 28;
            } else if ((pad.buttons & SCE_CTRL_UP) && (pad.buttons & SCE_CTRL_RIGHT)) {
                cur_btn_x = DPAD_CENTER_X + 28; cur_btn_y = DPAD_CENTER_Y - 28;
            } else if ((pad.buttons & SCE_CTRL_DOWN) && (pad.buttons & SCE_CTRL_LEFT)) {
                cur_btn_x = DPAD_CENTER_X - 28; cur_btn_y = DPAD_CENTER_Y + 28;
            } else if ((pad.buttons & SCE_CTRL_DOWN) && (pad.buttons & SCE_CTRL_RIGHT)) {
                cur_btn_x = DPAD_CENTER_X + 28; cur_btn_y = DPAD_CENTER_Y + 28;
            } else if (pad.buttons & SCE_CTRL_UP) {
                cur_btn_x = DPAD_CENTER_X; cur_btn_y = DPAD_CENTER_Y - DPAD_RADIUS;
            } else if (pad.buttons & SCE_CTRL_DOWN) {
                cur_btn_x = DPAD_CENTER_X; cur_btn_y = DPAD_CENTER_Y + DPAD_RADIUS;
            } else if (pad.buttons & SCE_CTRL_LEFT) {
                cur_btn_x = DPAD_CENTER_X - DPAD_RADIUS; cur_btn_y = DPAD_CENTER_Y;
            } else if (pad.buttons & SCE_CTRL_RIGHT) {
                cur_btn_x = DPAD_CENTER_X + DPAD_RADIUS; cur_btn_y = DPAD_CENTER_Y;
            } else if (pad.buttons & (SCE_CTRL_CROSS | SCE_CTRL_CIRCLE)) {
                cur_btn_x = BTN_ATTACK_X; cur_btn_y = BTN_ATTACK_Y;
            } else if (pad.buttons & SCE_CTRL_SQUARE) {
                cur_btn_x = BTN_SKILL1_X; cur_btn_y = BTN_SKILL1_Y;
            } else if (pad.buttons & SCE_CTRL_TRIANGLE) {
                cur_btn_x = BTN_SKILL2_X; cur_btn_y = BTN_SKILL2_Y;
            } else if (pad.buttons & SCE_CTRL_R1) {
                cur_btn_x = BTN_SKILL3_X; cur_btn_y = BTN_SKILL3_Y;
            } else if (pad.buttons & SCE_CTRL_L1) {
                cur_btn_x = BTN_SLOT1_X; cur_btn_y = BTN_SLOT1_Y;
            } else if (pad.buttons & SCE_CTRL_SELECT) {
                cur_btn_x = BTN_MAP_X; cur_btn_y = BTN_MAP_Y;
            }

            if (cur_btn_x >= 0 && cur_btn_y >= 0) {
                int action = prev_btn_active ? 2 : 0;
                jniTouch_fn(&jni, (jobject)0x42424242, (jint)cur_btn_x, (jint)cur_btn_y, (jint)action);
                prev_btn_active = 1;
                prev_btn_x = cur_btn_x;
                prev_btn_y = cur_btn_y;
            } else if (prev_btn_active) {
                jniTouch_fn(&jni, (jobject)0x42424242, (jint)prev_btn_x, (jint)prev_btn_y, 1); // Up
                prev_btn_active = 0;
                prev_btn_x = -1;
                prev_btn_y = -1;
            }
        }

        prev_buttons = pad.buttons;

        // 4. Tick game engine and render frame
        uint64_t start_time = sceKernelGetProcessTimeWide();
        int sleepTime = jniRun_fn(&jni, (jobject)0x42424242, j_buffer, (jint)frame_count++);

        if (sleepTime == 0) {
            l_info("[Destinia] Engine requested exit (sleepTime = 0).");
            break;
        }

        // 5. Upload 400x240 RGB565 buffer to vitaGL texture
        glBindTexture(GL_TEXTURE_2D, game_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, GAME_W, GAME_H, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, framebuffer);

        // 6. Draw textured quad
        glClear(GL_COLOR_BUFFER_BIT);
        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);

        gl_swap();

        // 7. Framerate limiter
        uint64_t elapsed_us = sceKernelGetProcessTimeWide() - start_time;
        uint64_t target_us = (sleepTime > 0 && sleepTime < 1000) ? ((uint64_t)sleepTime * 1000) : 33333;

        if (elapsed_us < target_us) {
            sceKernelDelayThread((SceUInt)(target_us - elapsed_us));
        }
    }

    if (destroyGame_fn) {
        destroyGame_fn(&jni, (jobject)0x42424242);
    }
    audio_shutdown();

    sceKernelExitProcess(0);
    return 0;
}
