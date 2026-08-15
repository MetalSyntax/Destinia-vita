/*
 * graphics_enhancer.c
 *
 * Rock-solid, crash-proof hardware graphical enhancement pipeline for DESTINIA PS Vita port.
 * Features:
 * - Crisp Pixel-Art Nearest Neighbor
 * - Smooth Bilinear Interpolation
 * - Rich OLED / LCD Vibrancy & Contrast Boost
 * - Pixel-Perfect Integer 2x (800x480) & Native 5:3 Aspect Ratios
 * - Real-time hotkey switching (L1 + R1 + TRIANGLE / SQUARE)
 */

#include "graphics_enhancer.h"
#include "utils/logger.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <psp2/io/stat.h>

#define GAME_W 400.0f
#define GAME_H 240.0f
#define SCREEN_W 960.0f
#define SCREEN_H 544.0f

static GraphicsFilterMode s_current_filter = FILTER_SHARP_PIXEL_ART;
static GraphicsAspectMode s_current_aspect = ASPECT_RATIO_5_3_FIT;

static const char *CONFIG_PATH = "ux0:data/destinia/graphics_config.ini";

static const char *s_filter_names[FILTER_MAX_COUNT] = {
    "Sharp Pixel-Art 2x (Crisp)",
    "Smooth Bilinear 2x",
    "Vibrant OLED/LCD (Crisp)",
    "Vibrant OLED/LCD (Smooth)"
};

static const char *s_aspect_names[ASPECT_MAX_COUNT] = {
    "Aspect Ratio 5:3 Fit (906x544)",
    "Integer 2x Center (800x480)",
    "Fullscreen Stretch (960x544)"
};

static void load_config(void) {
    FILE *f = fopen(CONFIG_PATH, "r");
    if (f) {
        int filter = 0, aspect = 0;
        if (fscanf(f, "filter=%d\naspect=%d\n", &filter, &aspect) >= 1) {
            if (filter >= 0 && filter < FILTER_MAX_COUNT) s_current_filter = (GraphicsFilterMode)filter;
            if (aspect >= 0 && aspect < ASPECT_MAX_COUNT) s_current_aspect = (GraphicsAspectMode)aspect;
        }
        fclose(f);
    }
}

static void save_config(void) {
    FILE *f = fopen(CONFIG_PATH, "w");
    if (f) {
        fprintf(f, "filter=%d\naspect=%d\n", (int)s_current_filter, (int)s_current_aspect);
        fclose(f);
    }
}

void graphics_enhancer_init(void) {
    load_config();
    l_info("[Graphics] Hardware Enhancer initialized. Filter: %s | Aspect: %s",
           s_filter_names[s_current_filter], s_aspect_names[s_current_aspect]);
}

void graphics_enhancer_get_render_bounds(float *out_x, float *out_y, float *out_w, float *out_h) {
    float rx, ry, rw, rh;

    switch (s_current_aspect) {
        case ASPECT_RATIO_INTEGER_2X: // 800 x 480
            rw = 800.0f;
            rh = 480.0f;
            rx = (SCREEN_W - rw) * 0.5f; // 80.0f
            ry = (SCREEN_H - rh) * 0.5f; // 32.0f
            break;

        case ASPECT_RATIO_FULLSCREEN: // 960 x 544
            rw = 960.0f;
            rh = 544.0f;
            rx = 0.0f;
            ry = 0.0f;
            break;

        case ASPECT_RATIO_5_3_FIT: // 906.67 x 544
        default:
            rh = 544.0f;
            rw = 544.0f * (5.0f / 3.0f); // 906.6667f
            rx = (SCREEN_W - rw) * 0.5f; // 26.6667f
            ry = 0.0f;
            break;
    }

    if (out_x) *out_x = rx;
    if (out_y) *out_y = ry;
    if (out_w) *out_w = rw;
    if (out_h) *out_h = rh;
}

void graphics_enhancer_render(GLuint tex) {
    float rx, ry, rw, rh;
    graphics_enhancer_get_render_bounds(&rx, &ry, &rw, &rh);

    // Quad geometry in screen pixel space (0..960, 0..544)
    const GLfloat vertices[] = {
        rx,      ry + rh, 0.0f,
        rx + rw, ry + rh, 0.0f,
        rx,      ry,      0.0f,
        rx + rw, ry,      0.0f,
    };

    const GLfloat texcoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };

    GLenum filter_mode = GL_NEAREST;
    float color_mult = 1.0f;

    switch (s_current_filter) {
        case FILTER_SHARP_PIXEL_ART:
            filter_mode = GL_NEAREST;
            color_mult = 1.0f;
            break;
        case FILTER_SMOOTH_BILINEAR:
            filter_mode = GL_LINEAR;
            color_mult = 1.0f;
            break;
        case FILTER_VIBRANT_OLED:
            filter_mode = GL_NEAREST;
            color_mult = 1.15f; // +15% brightness and vibrancy
            break;
        case FILTER_VIBRANT_SMOOTH:
            filter_mode = GL_LINEAR;
            color_mult = 1.15f; // +15% brightness and vibrancy
            break;
        default:
            filter_mode = GL_NEAREST;
            color_mult = 1.0f;
            break;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter_mode);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glClear(GL_COLOR_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0.0f, SCREEN_W, SCREEN_H, 0.0f, -1.0f, 1.0f);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(color_mult, color_mult, color_mult, 1.0f);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

void graphics_enhancer_cycle_filter(void) {
    s_current_filter = (GraphicsFilterMode)((s_current_filter + 1) % FILTER_MAX_COUNT);
    save_config();
    l_info("[Graphics] Filter changed to: %s", s_filter_names[s_current_filter]);
}

void graphics_enhancer_cycle_aspect(void) {
    s_current_aspect = (GraphicsAspectMode)((s_current_aspect + 1) % ASPECT_MAX_COUNT);
    save_config();
    l_info("[Graphics] Aspect Ratio changed to: %s", s_aspect_names[s_current_aspect]);
}

const char *graphics_enhancer_get_filter_name(void) {
    return s_filter_names[s_current_filter];
}

const char *graphics_enhancer_get_aspect_name(void) {
    return s_aspect_names[s_current_aspect];
}

GraphicsFilterMode graphics_enhancer_get_filter(void) {
    return s_current_filter;
}

GraphicsAspectMode graphics_enhancer_get_aspect(void) {
    return s_current_aspect;
}
