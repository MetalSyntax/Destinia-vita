#ifndef GRAPHICS_ENHANCER_H
#define GRAPHICS_ENHANCER_H

#include <vitaGL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FILTER_SHARP_BILINEAR_2X = 0, // Sharp Bilinear: Preserves crisp pixel art without shimmering or blur
    FILTER_SCALE2X_HD,            // Scale2x: Intelligent edge rounding for 2D sprites
    FILTER_VIBRANT_2X,            // Vibrant 2x: Sharp Bilinear + Color saturation & contrast boost (OLED/LCD)
    FILTER_INTEGER_2X,            // Integer 2x: 1:1 pixel crisp scaling
    FILTER_SMOOTH_LINEAR,         // Smooth Bilinear interpolation
    FILTER_CLASSIC_NEAREST,       // Classic unscaled nearest neighbor
    FILTER_MAX_COUNT
} GraphicsFilterMode;

typedef enum {
    ASPECT_RATIO_5_3_FIT = 0,     // 906.67 x 544: Correct native 5:3 aspect ratio fitting full height
    ASPECT_RATIO_INTEGER_2X,      // 800 x 480: Exact 2x integer scaling centered
    ASPECT_RATIO_FULLSCREEN,      // 960 x 544: Fullscreen fill
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
