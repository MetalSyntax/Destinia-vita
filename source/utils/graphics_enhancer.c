/*
 * graphics_enhancer.c
 *
 * Advanced graphical enhancement pipeline for DESTINIA PS Vita port.
 * Provides Sharp Bilinear 2x upscaling, Scale2x sprite edge smoothing,
 * color vibrancy/contrast tuning, integer scaling, and real-time filter cycling.
 */

#include "graphics_enhancer.h"
#include "utils/logger.h"
#include "utils/utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <psp2/io/stat.h>

#define GAME_W 400.0f
#define GAME_H 240.0f
#define SCREEN_W 960.0f
#define SCREEN_H 544.0f

static GraphicsFilterMode s_current_filter = FILTER_SHARP_BILINEAR_2X;
static GraphicsAspectMode s_current_aspect = ASPECT_RATIO_5_3_FIT;

static const char *CONFIG_PATH = "ux0:data/destinia/graphics_config.ini";

static GLuint s_prog_sharp = 0;
static GLuint s_prog_scale2x = 0;
static GLuint s_prog_vibrant = 0;
static bool s_shaders_available = false;

static const char *s_filter_names[FILTER_MAX_COUNT] = {
    "Sharp Bilinear 2x (Crisp Pixel-Art)",
    "Scale2x HD (Sprite Smoothing)",
    "Vibrant 2x (Rich OLED/LCD Color)",
    "Integer Scale 2x (Pixel-Perfect 800x480)",
    "Smooth Bilinear",
    "Classic Nearest Neighbor"
};

static const char *s_aspect_names[ASPECT_MAX_COUNT] = {
    "Aspect Ratio 5:3 Fit (906x544)",
    "Integer 2x Center (800x480)",
    "Fullscreen Stretch (960x544)"
};

/*
 * GLSL Shaders
 */

static const char *vshader_src =
    "attribute vec3 a_position;\n"
    "attribute vec2 a_texcoord;\n"
    "varying vec2 v_texcoord;\n"
    "void main() {\n"
    "    gl_Position = vec4(a_position, 1.0);\n"
    "    v_texcoord = a_texcoord;\n"
    "}\n";

// 1. Sharp Bilinear: Prescales integer 2x on GPU and subpixel interpolates smoothly
static const char *fshader_sharp_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_source_size;\n"
    "uniform vec2 u_target_size;\n"
    "void main() {\n"
    "    vec2 texel = v_texcoord * u_source_size;\n"
    "    vec2 texel_floor = floor(texel);\n"
    "    vec2 s = fract(texel);\n"
    "    vec2 region = 0.5 * (u_target_size / u_source_size);\n"
    "    vec2 subpixel = clamp((s - 0.5 + region) / max(region, vec2(0.001)), 0.0, 1.0);\n"
    "    subpixel = subpixel * subpixel * (3.0 - 2.0 * subpixel);\n"
    "    vec2 uv = (texel_floor + subpixel) / u_source_size;\n"
    "    gl_FragColor = texture2D(u_texture, uv);\n"
    "}\n";

// 2. Scale2x (EPX): Smart 2x directional sprite edge smoothing
static const char *fshader_scale2x_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_source_size;\n"
    "void main() {\n"
    "    vec2 dx = vec2(1.0 / u_source_size.x, 0.0);\n"
    "    vec2 dy = vec2(0.0, 1.0 / u_source_size.y);\n"
    "    vec2 fp = fract(v_texcoord * u_source_size);\n"
    "    vec2 center_uv = (floor(v_texcoord * u_source_size) + 0.5) / u_source_size;\n"
    "    vec4 E = texture2D(u_texture, center_uv);\n"
    "    vec4 B = texture2D(u_texture, center_uv - dy);\n"
    "    vec4 D = texture2D(u_texture, center_uv - dx);\n"
    "    vec4 F = texture2D(u_texture, center_uv + dx);\n"
    "    vec4 H = texture2D(u_texture, center_uv + dy);\n"
    "    vec4 res = E;\n"
    "    if (B != H && D != F) {\n"
    "        if (fp.x < 0.5 && fp.y < 0.5) {\n"
    "            res = (D == B) ? D : E;\n"
    "        } else if (fp.x >= 0.5 && fp.y < 0.5) {\n"
    "            res = (B == F) ? F : E;\n"
    "        } else if (fp.x < 0.5 && fp.y >= 0.5) {\n"
    "            res = (D == H) ? D : E;\n"
    "        } else {\n"
    "            res = (H == F) ? F : E;\n"
    "        }\n"
    "    }\n"
    "    gl_FragColor = res;\n"
    "}\n";

// 3. Vibrant: Sharp Bilinear + Color Vibrance & Gamma Boost
static const char *fshader_vibrant_src =
    "precision mediump float;\n"
    "varying vec2 v_texcoord;\n"
    "uniform sampler2D u_texture;\n"
    "uniform vec2 u_source_size;\n"
    "uniform vec2 u_target_size;\n"
    "void main() {\n"
    "    vec2 texel = v_texcoord * u_source_size;\n"
    "    vec2 texel_floor = floor(texel);\n"
    "    vec2 s = fract(texel);\n"
    "    vec2 region = 0.5 * (u_target_size / u_source_size);\n"
    "    vec2 subpixel = clamp((s - 0.5 + region) / max(region, vec2(0.001)), 0.0, 1.0);\n"
    "    subpixel = subpixel * subpixel * (3.0 - 2.0 * subpixel);\n"
    "    vec2 uv = (texel_floor + subpixel) / u_source_size;\n"
    "    vec4 c = texture2D(u_texture, uv);\n"
    "    vec3 col = c.rgb;\n"
    "    float luma = dot(col, vec3(0.299, 0.587, 0.114));\n"
    "    col = mix(vec3(luma), col, 1.15);\n" // 15% saturation enhancement
    "    col = pow(col, vec3(0.94));\n"       // subtle gamma contrast boost
    "    gl_FragColor = vec4(clamp(col, 0.0, 1.0), c.a);\n"
    "}\n";

static GLuint compile_shader_program(const char *vsrc, const char *fsrc) {
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, NULL);
    glCompileShader(vs);

    GLint compiled = 0;
    glGetShaderiv(vs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        l_warn("[Graphics] Vertex shader compilation failed.");
        glDeleteShader(vs);
        return 0;
    }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, NULL);
    glCompileShader(fs);

    glGetShaderiv(fs, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        l_warn("[Graphics] Fragment shader compilation failed.");
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);

    glBindAttribLocation(prog, 0, "a_position");
    glBindAttribLocation(prog, 1, "a_texcoord");

    glLinkProgram(prog);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        l_warn("[Graphics] Shader program link failed.");
        glDeleteProgram(prog);
        glDeleteShader(vs);
        glDeleteShader(fs);
        return 0;
    }

    return prog;
}

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
    sceIoMkdir("ux0:data", 0777);
    sceIoMkdir("ux0:data/shader_cache", 0777);
    sceIoMkdir("ux0:data/shader_cache/DESTINIA1", 0777);
    sceIoMkdir("ux0:data/destinia", 0777);

    load_config();

    s_prog_sharp = compile_shader_program(vshader_src, fshader_sharp_src);
    s_prog_scale2x = compile_shader_program(vshader_src, fshader_scale2x_src);
    s_prog_vibrant = compile_shader_program(vshader_src, fshader_vibrant_src);

    if (s_prog_sharp || s_prog_scale2x || s_prog_vibrant) {
        s_shaders_available = true;
        l_info("[Graphics] Hardware Shaders initialized successfully.");
    } else {
        l_warn("[Graphics] Custom shaders unavailable, using hardware texture filtering fallback.");
    }

    l_info("[Graphics] Active Filter: %s | Aspect: %s",
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

    // Convert pixel coordinates (0..960, 0..544) to Normalized Device Coordinates (-1..1)
    float x0 = (rx / SCREEN_W) * 2.0f - 1.0f;
    float x1 = ((rx + rw) / SCREEN_W) * 2.0f - 1.0f;
    float y0 = 1.0f - (ry / SCREEN_H) * 2.0f;
    float y1 = 1.0f - ((ry + rh) / SCREEN_H) * 2.0f;

    const GLfloat vertices[] = {
        x0, y1, 0.0f,
        x1, y1, 0.0f,
        x0, y0, 0.0f,
        x1, y0, 0.0f,
    };

    const GLfloat texcoords[] = {
        0.0f, 1.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        1.0f, 0.0f,
    };

    GLuint active_prog = 0;
    bool use_linear_tex = true;

    switch (s_current_filter) {
        case FILTER_SHARP_BILINEAR_2X:
            active_prog = s_prog_sharp;
            use_linear_tex = true;
            break;
        case FILTER_SCALE2X_HD:
            active_prog = s_prog_scale2x;
            use_linear_tex = false; // Nearest neighbor sampling for Scale2x
            break;
        case FILTER_VIBRANT_2X:
            active_prog = s_prog_vibrant;
            use_linear_tex = true;
            break;
        case FILTER_INTEGER_2X:
        case FILTER_CLASSIC_NEAREST:
            active_prog = 0;
            use_linear_tex = false;
            break;
        case FILTER_SMOOTH_LINEAR:
        default:
            active_prog = 0;
            use_linear_tex = true;
            break;
    }

    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, use_linear_tex ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, use_linear_tex ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glClear(GL_COLOR_BUFFER_BIT);

    if (active_prog && s_shaders_available) {
        glUseProgram(active_prog);

        GLint loc_tex = glGetUniformLocation(active_prog, "u_texture");
        if (loc_tex >= 0) glUniform1i(loc_tex, 0);

        GLint loc_src = glGetUniformLocation(active_prog, "u_source_size");
        if (loc_src >= 0) glUniform2f(loc_src, GAME_W, GAME_H);

        GLint loc_dst = glGetUniformLocation(active_prog, "u_target_size");
        if (loc_dst >= 0) glUniform2f(loc_dst, rw, rh);

        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, vertices);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableVertexAttribArray(0);
        glDisableVertexAttribArray(1);
        glUseProgram(0);
    } else {
        // Fixed-function fallback
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glEnableClientState(GL_VERTEX_ARRAY);
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glVertexPointer(3, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, texcoords);

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glDisableClientState(GL_VERTEX_ARRAY);
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    }
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
