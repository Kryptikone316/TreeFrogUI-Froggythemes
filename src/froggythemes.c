/* Froggythemes: a small TreeFrogUI background-pack selector for SF3000-class
 * devices.  It changes only the front end's settings file; the packs remain
 * installed in /mnt/sdcard/frogui/theme-packs/. */
#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#include <jpeglib.h>
#include <png.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "hwdisp.h"
#include "joykey.h"
#include "tfenv.h"

#define SDCARD "/mnt/sdcard"
#define PACK_DIR SDCARD "/frogui/theme-packs"
#define ICON_PACK_DIR SDCARD "/frogui/icon-packs"
#define SETTINGS SDCARD "/frogui/settings.txt"
#define ACTIVE_ICON SDCARD "/roms/ports/froggythemes/active-frog.rgba"
#define MAX_PACKS 64
#define MAX_NAME 96
#define ACTIVE_ICON_SIZE 32
#define ICON_SAMPLE_COUNT 6

enum {
    KEY_LEFT, KEY_RIGHT, KEY_UP, KEY_DOWN, KEY_A, KEY_B,
    KEY_L1, KEY_R1, KEY_X, KEY_Y, KEY_START, KEY_SELECT, KEY_COUNT
};

enum {
    TAB_BACKGROUNDS,
    TAB_ICONS,
    TAB_PRESETS,
    TAB_COUNT
};

struct preset {
    const char *name;
    const char *theme;
    const char *theme_pack;
    const char *style;
    const char *icon_pack;
};

static const struct preset g_presets[] = {
    { "Base Vertical", "Catppuccin Mocha", "", "vertical", "Pixel_by_Jeltron" },
    { "Base Horizontal", "Catppuccin Mocha", "", "horizontal", "Pixel_by_Jeltron" },
    { "Base System View", "Catppuccin Mocha", "", "system", "Pixel_by_Jeltron" },
    { "PrettyTreeFrog", "", "PrettyTreeFrog", "system", "PrettyTreeFrog" },
    { "Alekfull ARTFLIX", "", "EmuELEC_Alekfull_ARTFLIX", "system", "EmuELEC_Alekfull_ARTFLIX" },
    { "Alekfull NX", "", "EmuELEC_Alekfull_NX", "system", "EmuELEC_Alekfull_NX" },
    { "Alekfull SOLO", "", "EmuELEC_Alekfull_SOLO", "system", "EmuELEC_Alekfull_SOLO" },
    { "Carbon", "", "EmuELEC_Carbon", "system", "EmuELEC_Carbon" },
    { "Clean Style", "", "EmuELEC_Clean_Style", "system", "EmuELEC_Clean_Style" },
    { "Comic Book", "", "EmuELEC_Comic_Book", "system", "EmuELEC_Comic_Book" },
    { "Crystal Blue", "", "EmuELEC_Crystal_Blue", "system", "EmuELEC_Crystal_Blue" },
    { "Fundamental", "", "EmuELEC_Fundamental", "system", "EmuELEC_Fundamental" },
    { "Locomotion", "", "EmuELEC_Locomotion", "system", "EmuELEC_Locomotion" },
    { "NES Box", "", "EmuELEC_NES_Box", "system", "EmuELEC_NES_Box" },
    { "NES Mini", "", "EmuELEC_NES_Mini", "system", "EmuELEC_NES_Mini" },
    { "Neon Overdrive", "", "EmuELEC_Neon_Overdrive", "system", "EmuELEC_Neon_Overdrive" },
    { "Neon Pink Blue", "", "EmuELEC_Neon_Pink_Blue", "system", "EmuELEC_Neon_Pink_Blue" },
    { "Pixel", "", "EmuELEC_Pixel", "system", "EmuELEC_Pixel" },
    { "Roleta", "", "EmuELEC_Roleta", "system", "EmuELEC_Roleta" },
    { "Space Oddity", "", "EmuELEC_Space_Oddity", "system", "EmuELEC_Space_Oddity" },
    { "Spin Master", "", "EmuELEC_Spin_Master", "system", "EmuELEC_Spin_Master" },
    { "Super Sweet", "", "EmuELEC_Super_Sweet", "system", "EmuELEC_Super_Sweet" }
};

#define PRESET_COUNT ((int)(sizeof(g_presets) / sizeof(g_presets[0])))
#define BASE_PRESET_COUNT 3
#define PRESET_SECTION_HEIGHT 24

static int g_w = 854;
static int g_h = 480;
static uint16_t *g_canvas;
static uint16_t *g_backdrop;
static FT_Library g_ft;
static FT_Face g_face;
static int g_font_px = 22;
static volatile int g_quit;
static char g_status[160];
static int g_status_ok;
static char g_packs[MAX_PACKS][MAX_NAME];
static int g_pack_count;
static int g_selected;
static char g_current[MAX_NAME];
static char g_icon_packs[MAX_PACKS][MAX_NAME];
static int g_icon_count;
static int g_icon_selected;
static char g_current_icon[MAX_NAME];
static int g_tab;
static int g_preset_selected;
static char g_current_theme[MAX_NAME];
static char g_current_style[MAX_NAME];
static char g_current_backgrounds[MAX_NAME];
static int g_list_x, g_list_w;
static int g_preview_x, g_preview_y, g_preview_w, g_preview_h;
static uint16_t *g_preview;
static int g_preview_valid;
static char g_preview_pack[MAX_NAME];
static int g_icon_preview_valid;
static char g_icon_preview_pack[MAX_NAME];
static int g_preview_mode = -1;
static unsigned char g_active_icon[ACTIVE_ICON_SIZE * ACTIVE_ICON_SIZE * 4];
static int g_active_icon_loaded;

static uint16_t rgb(int r, int g, int b)
{
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

static void stop_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

static void fill_rect(int x, int y, int w, int h, uint16_t color)
{
    int yy, xx;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_w) w = g_w - x;
    if (y + h > g_h) h = g_h - y;
    if (w <= 0 || h <= 0) return;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++)
            g_canvas[yy * g_w + xx] = color;
}

static int glyph(unsigned c, int render)
{
    FT_UInt index;
    if (!g_face) return -1;
    index = FT_Get_Char_Index(g_face, c);
    if (FT_Load_Glyph(g_face, index, FT_LOAD_DEFAULT)) return -1;
    if (render && FT_Render_Glyph(g_face->glyph, FT_RENDER_MODE_NORMAL)) return -1;
    return 0;
}

static int text_width(const char *text)
{
    int width = 0;
    while (*text) {
        if (glyph((unsigned char)*text++, 0) == 0)
            width += (int)(g_face->glyph->advance.x >> 6);
    }
    return width;
}

static void text(int x, int baseline, uint16_t color, const char *value)
{
    int pen = x;
    while (*value) {
        int bx, by;
        if (glyph((unsigned char)*value++, 1)) continue;
        for (by = 0; by < (int)g_face->glyph->bitmap.rows; by++) {
            for (bx = 0; bx < (int)g_face->glyph->bitmap.width; bx++) {
                int px = pen + g_face->glyph->bitmap_left + bx;
                int py = baseline - g_face->glyph->bitmap_top + by;
                int alpha;
                uint16_t dst;
                int dr, dg, db, sr, sg, sb;
                if (px < 0 || px >= g_w || py < 0 || py >= g_h) continue;
                alpha = g_face->glyph->bitmap.buffer[by * g_face->glyph->bitmap.pitch + bx];
                if (alpha >= 250) { g_canvas[py * g_w + px] = color; continue; }
                if (!alpha) continue;
                dst = g_canvas[py * g_w + px];
                dr = (dst >> 11) & 31; dg = (dst >> 5) & 63; db = dst & 31;
                sr = (color >> 11) & 31; sg = (color >> 5) & 63; sb = color & 31;
                dr += (sr - dr) * alpha / 255;
                dg += (sg - dg) * alpha / 255;
                db += (sb - db) * alpha / 255;
                g_canvas[py * g_w + px] = (uint16_t)((dr << 11) | (dg << 5) | db);
            }
        }
        pen += (int)(g_face->glyph->advance.x >> 6);
    }
}

static void text_fit(int x, int baseline, int max_width, uint16_t color, const char *value)
{
    char copy[160];
    size_t n = strlen(value);
    if (n >= sizeof(copy)) n = sizeof(copy) - 1;
    memcpy(copy, value, n);
    copy[n] = 0;
    while (n > 3 && text_width(copy) > max_width) {
        n--;
        copy[n] = 0;
        if (n > 2) { copy[n - 1] = '.'; copy[n - 2] = '.'; }
    }
    text(x, baseline, color, copy);
}

static void line(int x0, int y0, int x1, int y1, uint16_t color)
{
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        if (x0 >= 0 && x0 < g_w && y0 >= 0 && y0 < g_h)
            g_canvas[y0 * g_w + x0] = color;
        if (x0 == x1 && y0 == y1) break;
        if (2 * err >= dy) { err += dy; x0 += sx; }
        if (2 * err <= dx) { err += dx; y0 += sy; }
    }
}

static void fill_circle(int cx, int cy, int radius, uint16_t color)
{
    int x, y;
    for (y = -radius; y <= radius; y++) {
        for (x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius &&
                cx + x >= 0 && cx + x < g_w && cy + y >= 0 && cy + y < g_h)
                g_canvas[(cy + y) * g_w + cx + x] = color;
        }
    }
}

static void fallback_active_icon(int cx, int cy)
{
    uint16_t accent = rgb(99, 181, 151), white = rgb(245, 255, 247);
    fill_circle(cx, cy, 10, accent);
    line(cx - 5, cy, cx - 1, cy + 4, white);
    line(cx - 1, cy + 4, cx + 6, cy - 5, white);
}

static int load_active_icon(void)
{
    FILE *file = fopen(ACTIVE_ICON, "rb");
    size_t expected = sizeof(g_active_icon);
    if (!file) return -1;
    if (fread(g_active_icon, 1, expected, file) != expected) {
        fclose(file);
        return -1;
    }
    fclose(file);
    g_active_icon_loaded = 1;
    return 0;
}

static void blend_active_icon(int cx, int cy)
{
    int x, y;
    if (!g_active_icon_loaded) {
        fallback_active_icon(cx, cy);
        return;
    }
    for (y = 0; y < ACTIVE_ICON_SIZE; y++) {
        for (x = 0; x < ACTIVE_ICON_SIZE; x++) {
            const unsigned char *src = &g_active_icon[(y * ACTIVE_ICON_SIZE + x) * 4];
            int px = cx - ACTIVE_ICON_SIZE / 2 + x;
            int py = cy - ACTIVE_ICON_SIZE / 2 + y;
            int alpha = src[3];
            uint16_t dst;
            int dr, dg, db;
            if (!alpha || px < 0 || px >= g_w || py < 0 || py >= g_h) continue;
            if (alpha >= 250) {
                g_canvas[py * g_w + px] = rgb(src[0], src[1], src[2]);
                continue;
            }
            dst = g_canvas[py * g_w + px];
            dr = ((dst >> 11) & 31) * 255 / 31;
            dg = ((dst >> 5) & 63) * 255 / 63;
            db = (dst & 31) * 255 / 31;
            dr += (src[0] - dr) * alpha / 255;
            dg += (src[1] - dg) * alpha / 255;
            db += (src[2] - db) * alpha / 255;
            g_canvas[py * g_w + px] = rgb(dr, dg, db);
        }
    }
}

static uint16_t blend_rgb565(uint16_t dst, int sr, int sg, int sb, int alpha)
{
    int dr = ((dst >> 11) & 31) * 255 / 31;
    int dg = ((dst >> 5) & 63) * 255 / 63;
    int db = (dst & 31) * 255 / 31;
    dr += (sr - dr) * alpha / 255;
    dg += (sg - dg) * alpha / 255;
    db += (sb - db) * alpha / 255;
    return rgb(dr, dg, db);
}

static void tint_rect(int x, int y, int w, int h, uint16_t color, int alpha)
{
    int yy, xx;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_w) w = g_w - x;
    if (y + h > g_h) h = g_h - y;
    if (w <= 0 || h <= 0) return;
    for (yy = y; yy < y + h; yy++)
        for (xx = x; xx < x + w; xx++)
            g_canvas[yy * g_w + xx] = blend_rgb565(
                g_canvas[yy * g_w + xx],
                ((color >> 11) & 31) * 255 / 31,
                ((color >> 5) & 63) * 255 / 63,
                (color & 31) * 255 / 31, alpha);
}

static void box_frame(int x, int y, int w, int h, uint16_t edge)
{
    line(x, y, x + w - 1, y, edge);
    line(x, y + h - 1, x + w - 1, y + h - 1, edge);
    line(x, y, x, y + h - 1, edge);
    line(x + w - 1, y, x + w - 1, y + h - 1, edge);
}

static void folder_tab(int x, int y, int w, int h,
                       uint16_t fill, uint16_t edge)
{
    int lip_w = w / 3;
    fill_rect(x, y, w, h, fill);
    fill_rect(x + 10, y - 6, lip_w, 6, fill);
    line(x + 10, y - 6, x + 10 + lip_w, y - 6, edge);
    line(x + 10, y - 6, x + 10, y, edge);
    box_frame(x, y, w, h, edge);
}

static void folder_tab_label(int text_x, const char *label,
                             uint16_t fill, uint16_t edge)
{
    const int pad_x = 10;
    folder_tab(text_x - pad_x, 12, text_width(label) + pad_x * 2, 46,
               fill, edge);
}

static void build_backdrop(void)
{
    int y, x;
    const int cyan_x = g_w / 5, cyan_y = g_h / 6;
    const int green_x = (g_w * 4) / 5, green_y = (g_h * 4) / 5;
    const int glow_radius = g_w / 2;
    const int glow_sq = glow_radius * glow_radius;
    if (!g_backdrop) return;
    for (y = 0; y < g_h; y++) {
        int t = g_h > 1 ? y * 255 / (g_h - 1) : 0;
        for (x = 0; x < g_w; x++) {
            int r = 3 + (3 * t / 255);
            int g = 12 + (8 * t / 255);
            int b = 27 + (10 * t / 255);
            int dx = x - cyan_x, dy = y - cyan_y;
            int distance = dx * dx + dy * dy;
            if (distance < glow_sq) {
                int a = (glow_sq - distance) * 28 / glow_sq;
                r += 0;
                g += 28 * a / 255;
                b += 72 * a / 255;
            }
            dx = x - green_x;
            dy = y - green_y;
            distance = dx * dx + dy * dy;
            if (distance < glow_sq) {
                int a = (glow_sq - distance) * 18 / glow_sq;
                g += 34 * a / 255;
                b += 13 * a / 255;
            }
            g_backdrop[y * g_w + x] = rgb(r, g, b);
        }
    }
}

struct png_image_rgba {
    unsigned char *pixels;
    int width;
    int height;
    size_t stride;
};

static void free_png_image(struct png_image_rgba *image)
{
    if (!image) return;
    free(image->pixels);
    memset(image, 0, sizeof(*image));
}

static int load_png_rgba(const char *path, struct png_image_rgba *image)
{
    FILE *file = NULL;
    png_structp png = NULL;
    png_infop info = NULL;
    png_bytep *rows = NULL;
    unsigned char *pixels = NULL;
    png_uint_32 width, height;
    int color_type, bit_depth, has_alpha;
    size_t stride;
    int y;

    if (!path || !image) return -1;
    memset(image, 0, sizeof(*image));
    file = fopen(path, "rb");
    if (!file) return -1;
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) { fclose(file); return -1; }
    info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(file);
        return -1;
    }
    if (setjmp(png_jmpbuf(png))) {
        free(rows);
        free(pixels);
        png_destroy_read_struct(&png, &info, NULL);
        fclose(file);
        return -1;
    }

    png_init_io(png, file);
    png_read_info(png, info);
    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);
    has_alpha = (color_type & PNG_COLOR_MASK_ALPHA) != 0 ||
                png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
        png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(png);
    if (!has_alpha) png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    png_read_update_info(png, info);
    stride = png_get_rowbytes(png, info);
    if (!width || !height || stride < (size_t)width * 4) goto fail;
    pixels = (unsigned char *)malloc(stride * height);
    rows = (png_bytep *)malloc(sizeof(*rows) * height);
    if (!pixels || !rows) goto fail;
    for (y = 0; y < (int)height; y++) rows[y] = pixels + (size_t)y * stride;
    png_read_image(png, rows);
    png_read_end(png, NULL);
    free(rows);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    image->pixels = pixels;
    image->width = (int)width;
    image->height = (int)height;
    image->stride = stride;
    return 0;

fail:
    free(rows);
    free(pixels);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(file);
    return -1;
}

static int load_icon_asset(const char *pack, const char *filename,
                           struct png_image_rgba *image)
{
    char path[512];
    const char *root = strcmp(pack, "default") == 0 ?
                       SDCARD "/frogui/system-icons" : ICON_PACK_DIR;
    if (strcmp(pack, "default") == 0)
        snprintf(path, sizeof(path), "%s/%s", root, filename);
    else
        snprintf(path, sizeof(path), "%s/%s/%s", root, pack, filename);
    if (load_png_rgba(path, image) == 0) return 0;
    if (strcmp(pack, "default") != 0) {
        snprintf(path, sizeof(path), "%s/%s", SDCARD "/frogui/system-icons", filename);
        if (load_png_rgba(path, image) == 0) return 0;
    }
    return -1;
}

struct icon_sample {
    const char *filename;
    const char *label;
};

static const struct icon_sample g_icon_samples[ICON_SAMPLE_COUNT] = {
    { "nes.png", "NES" },
    { "snes.png", "SNES" },
    { "ps1.png", "PS1" },
    { "mgba.png", "GBA" },
    { "pce.png", "PCE" },
    { "menu.png", "MENU" }
};

static void icon_cell(int index, int *x, int *y, int *w, int *h)
{
    int cell_w = g_preview_w / 3;
    int cell_h = g_preview_h / 2;
    int col = index % 3;
    int row = index / 3;
    *x = col * cell_w + 5;
    *y = row * cell_h + 5;
    *w = cell_w - 10;
    *h = cell_h - 30;
}

static void clear_preview(uint16_t color)
{
    size_t i, count = (size_t)g_preview_w * (size_t)g_preview_h;
    for (i = 0; i < count; i++) g_preview[i] = color;
}

static void draw_png_in_preview(const struct png_image_rgba *image,
                                int dx, int dy, int dw, int dh)
{
    int draw_w = dw;
    int draw_h = image->height * draw_w / image->width;
    int x, y;
    if (draw_h > dh) {
        draw_h = dh;
        draw_w = image->width * draw_h / image->height;
    }
    dx += (dw - draw_w) / 2;
    dy += (dh - draw_h) / 2;
    for (y = 0; y < draw_h; y++) {
        int sy = y * image->height / draw_h;
        const unsigned char *src_row = image->pixels + (size_t)sy * image->stride;
        for (x = 0; x < draw_w; x++) {
            int sx = x * image->width / draw_w;
            const unsigned char *src = src_row + (size_t)sx * 4;
            int px = dx + x, py = dy + y;
            if (src[3] && px >= 0 && px < g_preview_w && py >= 0 && py < g_preview_h)
                g_preview[py * g_preview_w + px] =
                    blend_rgb565(g_preview[py * g_preview_w + px],
                                 src[0], src[1], src[2], src[3]);
        }
    }
}

static int load_icon_preview(const char *pack)
{
    int i, loaded = 0;
    clear_preview(rgb(28, 36, 48));
    for (i = 0; i < ICON_SAMPLE_COUNT; i++) {
        struct png_image_rgba image;
        int x, y, w, h;
        if (load_icon_asset(pack, g_icon_samples[i].filename, &image) != 0) continue;
        icon_cell(i, &x, &y, &w, &h);
        draw_png_in_preview(&image, x, y, w, h);
        free_png_image(&image);
        loaded++;
    }
    return loaded ? 0 : -1;
}

static void preview_layout(void)
{
    const int preview_inset = 8;
    int outer_preview_x;
    int max_h;
    g_list_x = 16;
    g_list_w = (g_w * 43) / 100;
    outer_preview_x = g_list_x + g_list_w + 16;
    g_preview_x = outer_preview_x + preview_inset;
    g_preview_y = 104 + preview_inset;
    g_preview_w = g_w - g_preview_x - 16 - preview_inset;
    g_preview_h = (g_preview_w * 9) / 16;
    max_h = g_h - 190;
    if (g_preview_h > max_h) g_preview_h = max_h;
    if (g_preview_w < 1) g_preview_w = 1;
    if (g_preview_h < 1) g_preview_h = 1;
}

struct jpeg_error_state {
    struct jpeg_error_mgr pub;
    jmp_buf jump;
};

static void jpeg_fail(j_common_ptr cinfo)
{
    struct jpeg_error_state *state = (struct jpeg_error_state *)cinfo->err;
    longjmp(state->jump, 1);
}

static int load_preview(const char *pack)
{
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_state error_state;
    FILE *file = NULL;
    unsigned char *row = NULL;
    JSAMPROW row_ptr;
    char path[512];
    int created = 0, next_dy = 0, sy;

    if (!g_preview || !pack || !pack[0]) return -1;
    memset(g_preview, 0, (size_t)g_preview_w * (size_t)g_preview_h * sizeof(*g_preview));
    snprintf(path, sizeof(path), "%s/%s/main.jpg", PACK_DIR, pack);
    file = fopen(path, "rb");
    if (!file) return -1;

    memset(&cinfo, 0, sizeof(cinfo));
    cinfo.err = jpeg_std_error(&error_state.pub);
    error_state.pub.error_exit = jpeg_fail;
    if (setjmp(error_state.jump)) {
        free(row);
        if (created) jpeg_destroy_decompress(&cinfo);
        if (file) fclose(file);
        return -1;
    }
    jpeg_create_decompress(&cinfo);
    created = 1;
    jpeg_stdio_src(&cinfo, file);
    if (jpeg_read_header(&cinfo, TRUE) != JPEG_HEADER_OK) goto fail;
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    if (cinfo.output_components != 3 || !cinfo.output_width || !cinfo.output_height)
        goto fail;
    row = (unsigned char *)malloc((size_t)cinfo.output_width * 3);
    if (!row) goto fail;

    while (cinfo.output_scanline < cinfo.output_height) {
        int end_dy;
        sy = (int)cinfo.output_scanline;
        row_ptr = row;
        jpeg_read_scanlines(&cinfo, &row_ptr, 1);
        end_dy = ((sy + 1) * g_preview_h) / (int)cinfo.output_height;
        if (end_dy > g_preview_h) end_dy = g_preview_h;
        while (next_dy < end_dy) {
            int dx;
            for (dx = 0; dx < g_preview_w; dx++) {
                int sx = dx * (int)cinfo.output_width / g_preview_w;
                unsigned char *src = row + (size_t)sx * 3;
                g_preview[next_dy * g_preview_w + dx] = rgb(src[0], src[1], src[2]);
            }
            next_dy++;
        }
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);
    free(row);
    return next_dy == g_preview_h ? 0 : -1;

fail:
    free(row);
    jpeg_destroy_decompress(&cinfo);
    fclose(file);
    return -1;
}

static void update_preview(void)
{
    if (!g_preview || g_selected < 0 || g_selected >= g_pack_count) return;
    if (g_preview_mode == TAB_BACKGROUNDS &&
        strcmp(g_preview_pack, g_packs[g_selected]) == 0) return;
    g_preview_valid = load_preview(g_packs[g_selected]) == 0;
    snprintf(g_preview_pack, sizeof(g_preview_pack), "%s", g_packs[g_selected]);
    g_preview_mode = TAB_BACKGROUNDS;
}

static void update_icon_preview(void)
{
    if (!g_preview || g_icon_selected < 0 || g_icon_selected >= g_icon_count) return;
    if (g_preview_mode == TAB_ICONS &&
        strcmp(g_icon_preview_pack, g_icon_packs[g_icon_selected]) == 0) return;
    g_icon_preview_valid = load_icon_preview(g_icon_packs[g_icon_selected]) == 0;
    snprintf(g_icon_preview_pack, sizeof(g_icon_preview_pack), "%s",
             g_icon_packs[g_icon_selected]);
    g_preview_mode = TAB_ICONS;
}

static int read_setting(const char *key, char *out, size_t out_size)
{
    FILE *file = fopen(SETTINGS, "r");
    char line[256];
    size_t key_len = strlen(key);
    if (!file) return -1;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            char *value = line + key_len + 1;
            value[strcspn(value, "\r\n")] = 0;
            snprintf(out, out_size, "%s", value);
            fclose(file);
            return 0;
        }
    }
    fclose(file);
    return -1;
}

static int replace_settings(const char *key, const char *value,
                            int enable_backgrounds, int enable_system_view)
{
    char tmp_path[sizeof(SETTINGS) + 5];
    FILE *in, *out;
    char line[512];
    size_t key_len = strlen(key);
    int seen_key = 0, seen_backgrounds = 0, seen_style = 0;

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", SETTINGS);
    in = fopen(SETTINGS, "r");
    if (!in) return -1;
    out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return -1; }
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, key, key_len) == 0 && line[key_len] == '=') {
            fprintf(out, "%s=%s\n", key, value);
            seen_key = 1;
        } else if (enable_backgrounds && strncmp(line, "backgrounds=", 12) == 0) {
            fputs("backgrounds=on\n", out);
            seen_backgrounds = 1;
        } else if (enable_system_view && strncmp(line, "style=", 6) == 0) {
            fputs("style=system\n", out);
            seen_style = 1;
        } else {
            fputs(line, out);
        }
    }
    if (!seen_key) fprintf(out, "%s=%s\n", key, value);
    if (enable_backgrounds && !seen_backgrounds) fputs("backgrounds=on\n", out);
    if (enable_system_view && !seen_style) fputs("style=system\n", out);
    if (fclose(in) != 0 || fclose(out) != 0 || rename(tmp_path, SETTINGS) != 0) {
        unlink(tmp_path);
        return -1;
    }
    sync();
    return 0;
}

static int apply_preset(const struct preset *preset)
{
    char tmp_path[sizeof(SETTINGS) + 5];
    FILE *in, *out;
    char line[512];
    int seen_theme = 0, seen_theme_pack = 0, seen_style = 0, seen_icon = 0;
    int seen_wallpaper = 0, seen_backgrounds = 0;
    int set_theme = preset->theme && preset->theme[0];
    int has_theme_pack = preset->theme_pack && preset->theme_pack[0];

    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", SETTINGS);
    in = fopen(SETTINGS, "r");
    if (!in) return -1;
    out = fopen(tmp_path, "w");
    if (!out) { fclose(in); return -1; }
    while (fgets(line, sizeof(line), in)) {
        if (strncmp(line, "theme=", 6) == 0) {
            if (set_theme)
                fprintf(out, "theme=%s\n", preset->theme);
            else
                fputs(line, out);
            seen_theme = 1;
        } else if (strncmp(line, "theme_pack=", 11) == 0) {
            if (has_theme_pack)
                fprintf(out, "theme_pack=%s\n", preset->theme_pack);
            seen_theme_pack = 1;
        } else if (strncmp(line, "style=", 6) == 0) {
            fprintf(out, "style=%s\n", preset->style);
            seen_style = 1;
        } else if (strncmp(line, "icon_pack=", 10) == 0) {
            fprintf(out, "icon_pack=%s\n", preset->icon_pack);
            seen_icon = 1;
        } else if (strncmp(line, "wallpaper=", 10) == 0) {
            fputs("wallpaper=none\n", out);
            seen_wallpaper = 1;
        } else if (strncmp(line, "backgrounds=", 12) == 0) {
            fputs(has_theme_pack ? "backgrounds=on\n" : "backgrounds=off\n", out);
            seen_backgrounds = 1;
        } else {
            fputs(line, out);
        }
    }
    if (!seen_theme && set_theme) fprintf(out, "theme=%s\n", preset->theme);
    if (has_theme_pack && !seen_theme_pack)
        fprintf(out, "theme_pack=%s\n", preset->theme_pack);
    if (!seen_style) fprintf(out, "style=%s\n", preset->style);
    if (!seen_icon) fprintf(out, "icon_pack=%s\n", preset->icon_pack);
    if (!seen_wallpaper) fputs("wallpaper=none\n", out);
    if (!seen_backgrounds)
        fputs(has_theme_pack ? "backgrounds=on\n" : "backgrounds=off\n", out);
    if (fclose(in) != 0 || fclose(out) != 0 || rename(tmp_path, SETTINGS) != 0) {
        unlink(tmp_path);
        return -1;
    }
    sync();
    return 0;
}

static int preset_is_active(const struct preset *preset)
{
    const char *theme_pack = preset->theme_pack ? preset->theme_pack : "";
    int theme_matches = !preset->theme || !preset->theme[0] ||
                        strcmp(g_current_theme, preset->theme) == 0;
    int has_theme_pack = theme_pack[0] != '\0';

    return theme_matches &&
           strcmp(g_current_style, preset->style) == 0 &&
           strcmp(g_current_icon, preset->icon_pack) == 0 &&
           strcmp(g_current, theme_pack) == 0 &&
           (has_theme_pack ? strcmp(g_current_backgrounds, "on") == 0 :
            (!g_current_backgrounds[0] || strcmp(g_current_backgrounds, "off") == 0));
}

static int pack_compare(const void *a, const void *b)
{
    return strcasecmp((const char *)a, (const char *)b);
}

/* Folder names are deliberately stable because they are written to
 * settings.txt, but the raw names are noisy on the small display.  Keep the
 * source prefix out of the row when it only repeats the tab's meaning. */
static void format_pack_name(const char *raw, char *out, size_t out_size)
{
    const char *name = raw ? raw : "";
    size_t i;

    if (strncasecmp(name, "EmuELEC_", 8) == 0) name += 8;
    else if (strncasecmp(name, "Batocera_", 9) == 0) name += 9;
    else if (strncasecmp(name, "muOS_", 5) == 0) name += 5;

    for (i = 0; name[i] && i + 1 < out_size; i++)
        out[i] = name[i] == '_' ? ' ' : name[i];
    if (out_size) out[i] = '\0';
    if (strcmp(raw ? raw : "", "default") == 0)
        snprintf(out, out_size, "System Default");
}

static int pack_name_exists(char names[MAX_PACKS][MAX_NAME], int count, const char *name)
{
    int i;
    for (i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) return 1;
    }
    return 0;
}

static int scan_pack_dir(const char *root, char names[MAX_PACKS][MAX_NAME])
{
    DIR *dir = opendir(root);
    struct dirent *entry;
    int count = 0;
    if (!dir) return 0;
    while ((entry = readdir(dir)) && count < MAX_PACKS) {
        char path[512];
        struct stat st;
        if (entry->d_name[0] == '.') continue;
        snprintf(path, sizeof(path), "%s/%s", root, entry->d_name);
        if (stat(path, &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(names[count++], MAX_NAME, "%s", entry->d_name);
    }
    closedir(dir);
    qsort(names, (size_t)count, sizeof(names[0]), pack_compare);
    return count;
}

static void scan_packs(void)
{
    g_pack_count = scan_pack_dir(PACK_DIR, g_packs);
    g_icon_count = scan_pack_dir(ICON_PACK_DIR, g_icon_packs);
    if (g_icon_count < MAX_PACKS &&
        !pack_name_exists(g_icon_packs, g_icon_count, "default")) {
        snprintf(g_icon_packs[g_icon_count++], MAX_NAME, "default");
        qsort(g_icon_packs, (size_t)g_icon_count,
              sizeof(g_icon_packs[0]), pack_compare);
    }
}

static int load_font(void)
{
    const char *fonts[] = {
        SDCARD "/frogui/fonts/TreeFrogLatin.ttf",
        SDCARD "/frogui/fonts/TreeFrogUnicode.ttf",
        SDCARD "/cubegm/font.ttf"
    };
    int i;
    if (FT_Init_FreeType(&g_ft)) return -1;
    for (i = 0; i < (int)(sizeof(fonts) / sizeof(fonts[0])); i++) {
        if (FT_New_Face(g_ft, fonts[i], 0, &g_face) == 0) {
            FT_Set_Pixel_Sizes(g_face, 0, (FT_UInt)g_font_px);
            return 0;
        }
    }
    return -1;
}

static void draw_screen(void)
{
    uint16_t accent = rgb(99, 210, 179), white = rgb(235, 239, 235);
    uint16_t muted = rgb(151, 171, 179), selected = rgb(29, 76, 88);
    uint16_t cyan_edge = rgb(0, 168, 214), green_edge = rgb(65, 170, 135);
    int item_count = g_tab == TAB_BACKGROUNDS ? g_pack_count :
                     g_tab == TAB_ICONS ? g_icon_count : PRESET_COUNT;
    int selected_item = g_tab == TAB_BACKGROUNDS ? g_selected :
                        g_tab == TAB_ICONS ? g_icon_selected : g_preset_selected;
    const char *current_item = g_tab == TAB_ICONS ? g_current_icon : g_current;
    int i, row_h = 44, first = 0, visible = 6;
    int tab_themes_x = (g_w * 45) / 100;
    int tab_icons_x = tab_themes_x + 112;
    int tab_presets_x = tab_themes_x + 202;
    const int list_y = 116; /* leave a compact catalog header below the tabs */
    const int preview_text_pad = 8;
    char line[180];
    char display_name[MAX_NAME];
    char count_line[48];

    if (g_tab == TAB_BACKGROUNDS) update_preview();
    else if (g_tab == TAB_ICONS) update_icon_preview();
    if (g_backdrop)
        memcpy(g_canvas, g_backdrop, (size_t)g_w * (size_t)g_h * sizeof(*g_canvas));
    else
        fill_rect(0, 0, g_w, g_h, rgb(5, 14, 27));
    tint_rect(12, 8, g_w - 24, 52, rgb(8, 39, 66), 225);
    box_frame(12, 8, g_w - 24, 52, cyan_edge);
    tint_rect(g_list_x, 88, g_list_w, 316, rgb(7, 31, 54), 205);
    box_frame(g_list_x, 88, g_list_w, 316, green_edge);
    tint_rect(g_preview_x - 8, 88, g_preview_w + 16, g_preview_h + 79,
              rgb(7, 31, 52), 205);
    box_frame(g_preview_x - 8, 88, g_preview_w + 16, g_preview_h + 79, cyan_edge);
    tint_rect(16, g_h - 60, g_w - 32, 48, rgb(8, 30, 49), 225);
    box_frame(16, g_h - 60, g_w - 32, 48, green_edge);
    if (g_tab == TAB_BACKGROUNDS)
        folder_tab_label(tab_themes_x, "THEMES", rgb(13, 62, 82), cyan_edge);
    else if (g_tab == TAB_ICONS)
        folder_tab_label(tab_icons_x, "ICONS", rgb(13, 62, 82), cyan_edge);
    else
        folder_tab_label(tab_presets_x, "PRESETS", rgb(13, 62, 82), cyan_edge);
    text(24, 32, accent, "Froggythemes");
    text(tab_themes_x, 32, g_tab == TAB_BACKGROUNDS ? white : muted, "THEMES");
    text(tab_icons_x, 32, g_tab == TAB_ICONS ? white : muted, "ICONS");
    text(tab_presets_x, 32, g_tab == TAB_PRESETS ? white : muted, "PRESETS");
    if (g_tab != TAB_PRESETS) {
        snprintf(count_line, sizeof(count_line), "%d available", item_count);
        text(g_list_x, 78, muted, count_line);
        snprintf(count_line, sizeof(count_line), "%d/%d", item_count ? selected_item + 1 : 0,
                 item_count);
        text_fit(g_list_x + g_list_w - 54, 78, 54, muted, count_line);
    }
    if (selected_item >= visible) first = selected_item - visible + 1;
    if (g_tab == TAB_PRESETS) {
        int header_y = list_y - 26;
        text(g_list_x, header_y, muted,
             first < BASE_PRESET_COUNT ? "BASE PRESETS" : "PACK PRESETS");
        fill_rect(g_list_x, header_y + 8, g_list_w, 1, rgb(54, 66, 76));
        if (first < BASE_PRESET_COUNT && first + visible > BASE_PRESET_COUNT) {
            header_y = list_y + (BASE_PRESET_COUNT - first) * row_h +
                       PRESET_SECTION_HEIGHT - 26;
            text(g_list_x, header_y, muted, "PACK PRESETS");
            fill_rect(g_list_x, header_y + 8, g_list_w, 1,
                      rgb(54, 66, 76));
        }
    }
    for (i = first; i < item_count && i < first + visible; i++) {
        int y = list_y + (i - first) * row_h;
        const char *item_name;
        int is_current;
        if (g_tab == TAB_PRESETS && first < BASE_PRESET_COUNT &&
            i >= BASE_PRESET_COUNT)
            y += PRESET_SECTION_HEIGHT;
        if (g_tab == TAB_BACKGROUNDS) item_name = g_packs[i];
        else if (g_tab == TAB_ICONS) item_name = g_icon_packs[i];
        else item_name = g_presets[i].name;
        format_pack_name(item_name, display_name, sizeof(display_name));
        is_current = g_tab == TAB_PRESETS ? preset_is_active(&g_presets[i]) :
                     strcmp(item_name, current_item) == 0;
        if (i == selected_item) {
            tint_rect(g_list_x, y - 28, g_list_w, 40, selected, 220);
            fill_rect(g_list_x, y - 28, 5, 40, accent);
        }
        text_fit(g_list_x + 36, y, g_list_w - 48,
                 i == selected_item ? white : muted, display_name);
        if (is_current) blend_active_icon(g_list_x + 20, y - 10);
    }
    if (item_count > visible) {
        int track_y = list_y - 28;
        int track_h = visible * row_h;
        int thumb_h = (track_h * visible) / item_count;
        int thumb_y;
        if (thumb_h < 16) thumb_h = 16;
        thumb_y = track_y + (track_h - thumb_h) * first / (item_count - visible);
        fill_rect(g_list_x + g_list_w - 5, track_y, 3, track_h, rgb(54, 66, 76));
        fill_rect(g_list_x + g_list_w - 5, thumb_y, 3, thumb_h, accent);
    }
    if (!item_count) {
        text(g_list_x, 120, white, g_tab == TAB_BACKGROUNDS ?
             "No background packs found." : "No icon packs found.");
        text(g_list_x, 150, muted, g_tab == TAB_BACKGROUNDS ?
             "Add folders under frogui/theme-packs." :
             "Add folders under frogui/icon-packs.");
    }

    if (g_tab == TAB_BACKGROUNDS) {
        text(g_preview_x + preview_text_pad, g_preview_y - 7, muted, "PREVIEW");
    }
    if (g_tab == TAB_BACKGROUNDS && g_preview_valid && g_preview) {
        int py;
        for (py = 0; py < g_preview_h; py++) {
            memcpy(g_canvas + (g_preview_y + py) * g_w + g_preview_x,
                   g_preview + py * g_preview_w,
                   (size_t)g_preview_w * sizeof(*g_preview));
        }
        format_pack_name(g_preview_pack, display_name, sizeof(display_name));
        text_fit(g_preview_x + preview_text_pad,
                 g_preview_y + g_preview_h + 26,
                 g_preview_w - preview_text_pad * 2, white, display_name);
    } else if (g_tab == TAB_BACKGROUNDS) {
        text(g_preview_x + 16, g_preview_y + g_preview_h / 2,
             muted, "No main.jpg preview");
    } else if (g_tab == TAB_ICONS && g_icon_preview_valid && g_preview) {
        int py, sample;
        text(g_preview_x + preview_text_pad, g_preview_y - 7, muted, "ICON PACK");
        for (py = 0; py < g_preview_h; py++) {
            memcpy(g_canvas + (g_preview_y + py) * g_w + g_preview_x,
                   g_preview + py * g_preview_w,
                   (size_t)g_preview_w * sizeof(*g_preview));
        }
        for (sample = 0; sample < ICON_SAMPLE_COUNT; sample++) {
            int x, y, w, h;
            icon_cell(sample, &x, &y, &w, &h);
            text_fit(g_preview_x + x + 4, g_preview_y + y + h + 21,
                     w - 8, rgb(188, 198, 201), g_icon_samples[sample].label);
        }
        format_pack_name(g_icon_preview_pack, display_name, sizeof(display_name));
        text_fit(g_preview_x + preview_text_pad,
                 g_preview_y + g_preview_h + 26,
                 g_preview_w - preview_text_pad * 2, white, display_name);
    } else if (g_tab == TAB_ICONS) {
        char count_line[64];
        text(g_preview_x + preview_text_pad, g_preview_y - 7, muted, "ICON PACK");
        text(g_preview_x + 16, g_preview_y + g_preview_h / 2,
             muted, "No icon samples found");
        snprintf(count_line, sizeof(count_line), "%d icon packs installed", item_count);
        text_fit(g_preview_x + preview_text_pad,
                 g_preview_y + g_preview_h + 26,
                 g_preview_w - preview_text_pad * 2, muted, count_line);
    } else {
        text(g_preview_x + preview_text_pad, g_preview_y - 7, muted, "PRESET");
        text_fit(g_preview_x + preview_text_pad, g_preview_y + 34,
                 g_preview_w - preview_text_pad * 2,
                 white, g_presets[selected_item].name);
        if (preset_is_active(&g_presets[selected_item]))
            text(g_preview_x + preview_text_pad, g_preview_y + 76, accent,
                 g_presets[selected_item].theme_pack[0] ?
                 "ACTIVE THEME + ICON" : "ACTIVE BASE LAYOUT");
        text_fit(g_preview_x + preview_text_pad, g_preview_y + 122,
                 g_preview_w - preview_text_pad * 2,
                 muted, g_presets[selected_item].theme_pack[0] ?
                 "Applies matching background and icon packs." :
                 "Restores TreeFrogUI base settings.");
        text_fit(g_preview_x + preview_text_pad, g_preview_y + 158,
                 g_preview_w - preview_text_pad * 2,
                 muted, "Installed packs are not deleted.");
    }
    if (g_status[0]) {
        text_fit(24, g_h - 72, g_w - 48, g_status_ok ? accent : rgb(236, 125, 110), g_status);
    }
    snprintf(line, sizeof(line), "L/R tabs  UP/DOWN select  A apply  B back  SEL+START exit");
    text_fit(24, g_h - 28, g_w - 48, muted, line);
}

static unsigned buttons(void)
{
    unsigned raw = joykey_read(), out = 0;
    if (raw & (1u << JK_LEFT)) out |= 1u << KEY_LEFT;
    if (raw & (1u << JK_RIGHT)) out |= 1u << KEY_RIGHT;
    if (raw & (1u << JK_UP)) out |= 1u << KEY_UP;
    if (raw & (1u << JK_DOWN)) out |= 1u << KEY_DOWN;
    if (raw & (1u << JK_A)) out |= 1u << KEY_A;
    if (raw & (1u << JK_B)) out |= 1u << KEY_B;
    if (raw & (1u << JK_L1)) out |= 1u << KEY_L1;
    if (raw & (1u << JK_R1)) out |= 1u << KEY_R1;
    if (raw & (1u << JK_X)) out |= 1u << KEY_X;
    if (raw & (1u << JK_Y)) out |= 1u << KEY_Y;
    if (raw & (1u << JK_START)) out |= 1u << KEY_START;
    if (raw & (1u << JK_SELECT)) out |= 1u << KEY_SELECT;
    return out;
}

int main(void)
{
    char panel_w[32], panel_h[32];
    unsigned previous = 0;
    int i;

    signal(SIGINT, stop_signal);
    signal(SIGTERM, stop_signal);
    if (tfenv_get("TF_PANEL_W", panel_w, sizeof(panel_w))) g_w = atoi(panel_w);
    if (tfenv_get("TF_PANEL_H", panel_h, sizeof(panel_h))) g_h = atoi(panel_h);
    if (g_w < 320 || g_w > 1920) g_w = 854;
    if (g_h < 240 || g_h > 1080) g_h = 480;
    if (g_h != 480) g_font_px = g_h * 22 / 480;

    if (read_setting("theme_pack", g_current, sizeof(g_current)) != 0)
        g_current[0] = 0;
    if (read_setting("theme", g_current_theme, sizeof(g_current_theme)) != 0)
        g_current_theme[0] = 0;
    if (read_setting("style", g_current_style, sizeof(g_current_style)) != 0)
        g_current_style[0] = 0;
    if (read_setting("backgrounds", g_current_backgrounds,
                     sizeof(g_current_backgrounds)) != 0)
        g_current_backgrounds[0] = 0;
    if (read_setting("icon_pack", g_current_icon, sizeof(g_current_icon)) != 0)
        g_current_icon[0] = 0;
    scan_packs();
    for (i = 0; i < g_pack_count; i++) {
        if (strcmp(g_packs[i], g_current) == 0) { g_selected = i; break; }
    }
    if (i == g_pack_count) g_selected = 0;
    for (i = 0; i < g_icon_count; i++) {
        if (strcmp(g_icon_packs[i], g_current_icon) == 0) {
            g_icon_selected = i;
            break;
        }
    }
    if (i == g_icon_count) g_icon_selected = 0;
    for (i = 0; i < PRESET_COUNT; i++) {
        if (preset_is_active(&g_presets[i])) {
            g_preset_selected = i;
            break;
        }
    }

    g_canvas = (uint16_t *)calloc((size_t)g_w * (size_t)g_h, sizeof(*g_canvas));
    g_backdrop = (uint16_t *)calloc((size_t)g_w * (size_t)g_h, sizeof(*g_backdrop));
    preview_layout();
    g_preview = (uint16_t *)calloc((size_t)g_preview_w * (size_t)g_preview_h,
                                   sizeof(*g_preview));
    load_active_icon();
    if (!g_canvas || !g_backdrop || !g_preview || load_font() != 0 || hwdisp_init() != 0) {
        fprintf(stderr, "froggythemes: display/font initialization failed\n");
        free(g_preview);
        free(g_backdrop);
        free(g_canvas);
        return 1;
    }
    build_backdrop();
    joykey_init();

    while (!g_quit) {
        unsigned held = buttons();
        unsigned press = held & ~previous;
        int item_count;
        previous = held;
        if (press & ((1u << KEY_B) | (1u << KEY_Y))) g_quit = 1;
        if ((held & (1u << KEY_SELECT)) && (held & (1u << KEY_START))) g_quit = 1;
        if (press & ((1u << KEY_LEFT) | (1u << KEY_L1)))
            g_tab = (g_tab + TAB_COUNT - 1) % TAB_COUNT;
        if (press & ((1u << KEY_RIGHT) | (1u << KEY_R1)))
            g_tab = (g_tab + 1) % TAB_COUNT;
        item_count = g_tab == TAB_BACKGROUNDS ? g_pack_count :
                     g_tab == TAB_ICONS ? g_icon_count : PRESET_COUNT;
        if ((press & (1u << KEY_UP)) && item_count) {
            if (g_tab == TAB_ICONS)
                g_icon_selected = (g_icon_selected + item_count - 1) % item_count;
            else if (g_tab == TAB_PRESETS)
                g_preset_selected = (g_preset_selected + item_count - 1) % item_count;
            else
                g_selected = (g_selected + item_count - 1) % item_count;
        }
        if ((press & (1u << KEY_DOWN)) && item_count) {
            if (g_tab == TAB_ICONS)
                g_icon_selected = (g_icon_selected + 1) % item_count;
            else if (g_tab == TAB_PRESETS)
                g_preset_selected = (g_preset_selected + 1) % item_count;
            else
                g_selected = (g_selected + 1) % item_count;
        }
        if ((press & (1u << KEY_A)) && item_count) {
            const char *name;
            const char *key;
            int enable_backgrounds, enable_system_view;
            if (g_tab == TAB_PRESETS) {
                if (apply_preset(&g_presets[g_preset_selected]) == 0) {
                    snprintf(g_status, sizeof(g_status),
                             "Applied %s. Returning to TreeFrogUI...",
                             g_presets[g_preset_selected].name);
                    g_status_ok = 1;
                    g_quit = 1;
                } else {
                    snprintf(g_status, sizeof(g_status),
                             "Could not save TreeFrogUI settings.");
                    g_status_ok = 0;
                }
                name = NULL;
                key = NULL;
                enable_backgrounds = 0;
                enable_system_view = 0;
            } else if (g_tab == TAB_ICONS) {
                name = g_icon_packs[g_icon_selected];
                key = "icon_pack";
                enable_backgrounds = 0;
                enable_system_view = 1;
            } else {
                name = g_packs[g_selected];
                key = "theme_pack";
                enable_backgrounds = 1;
                enable_system_view = 0;
            }
            if (g_tab == TAB_PRESETS) {
                /* Preset application and status handling are complete above. */
            } else if (replace_settings(key, name, enable_backgrounds, enable_system_view) == 0) {
                if (g_tab == TAB_ICONS)
                    snprintf(g_current_icon, sizeof(g_current_icon), "%s", name);
                else
                    snprintf(g_current, sizeof(g_current), "%s", name);
                snprintf(g_status, sizeof(g_status), "Applied %s. Returning to TreeFrogUI...", name);
                g_status_ok = 1;
                g_quit = 1;
            } else {
                snprintf(g_status, sizeof(g_status), "Could not save TreeFrogUI settings.");
                g_status_ok = 0;
            }
        }
        draw_screen();
        hwdisp_present(g_canvas, g_w, g_h, g_w * 2);
        usleep(16000);
    }

    hwdisp_deinit();
    if (g_face) FT_Done_Face(g_face);
    FT_Done_FreeType(g_ft);
    free(g_preview);
    free(g_backdrop);
    free(g_canvas);
    return 0;
}
