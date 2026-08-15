#ifndef GRAPHICS_ENHANCER_H
#define GRAPHICS_ENHANCER_H

#include <vitaGL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILTER_SHARP_PIXEL_ART = 0,   // Crisp 1:1 Pixel-Art Nearest Neighbor
    FILTER_SMOOTH_BILINEAR,       // Smooth Bilinear Texture Filter
    FILTER_VIBRANT_OLED,          // Rich Color Vibrancy & Contrast Boost (Sharp)
    FILTER_VIBRANT_SMOOTH,        // Rich Color Vibrancy & Contrast Boost (Smooth)
    FILTER_MAX_COUNT
} GraphicsFilterMode;

typedef enum {
    ASPECT_RATIO_5_3_FIT = 0,     // 906.67 x 544: Native 5:3 aspect ratio fitting full height
    ASPECT_RATIO_INTEGER_2X,      // 800 x 480: Exact 2x integer scaling centered
    ASPECT_RATIO_FULLSCREEN,      // 960 x 544: Fullscreen 16:9 stretch
    ASPECT_MAX_COUNT
} GraphicsAspectMode;

void graphics_enhancer_init(void);
void graphics_enhancer_render(GLuint tex);
void graphics_enhancer_cycle_filter(void);
void graphics_enhancer_cycle_aspect(void);
const char *graphics_enhancer_get_filter_name(void);
const char *graphics_enhancer_get_aspect_name(void);
GraphicsFilterMode graphics_enhancer_get_filter(void);
GraphicsAspectMode graphics_enhancer_get_aspect(void);
void graphics_enhancer_get_render_bounds(float *out_x, float *out_y, float *out_w, float *out_h);

#ifdef __cplusplus
}
#endif

#endif // GRAPHICS_ENHANCER_H
