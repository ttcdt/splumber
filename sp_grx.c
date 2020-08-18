/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#include "sp_supp.h"
#include "sp_types.h"
#include "sp_grx.h"

/* underwater virtual screen */
char *_water_virtual_screen = NULL;

/* original water texture (as loaded from disk) */
sp_texture *_original_water_texture;

/* the current water texture */
sp_texture *_water_texture;

/* texture buffers for water waving */
sp_texture *_wave_water_texture[2];

/* offset to water texture in use */
int _wave_water_texture_index = 0;

/* sin / cos tables for water textures */
int _water_texture_sin_table[TEXTURE_Y_SIZE];
int _water_texture_cos_table[TEXTURE_X_SIZE];

/* waving period */
int _water_texture_period = 0;

/* sin / cos tables for water screen */
int _water_screen_sin_table[2048];
int _water_screen_cos_table[2048];

/* waving period */
int _water_screen_period = 0;

void (*DrawMask) (char *);

/* the font */
char _font_file[150] = "etc/v-dub.ktl";

/* the underwater palette */
unsigned int _water_palette[256 * 3];

/* the original palette */
unsigned int _orig_palette[256 * 3];

int _fade_edges = 0;

/********************
    Code
*********************/

void LoadPalette(char *palfile)
{
    qdgdfv_load_palette(palfile);
    qdgdfv_set_palette();
}


void LoadScreen(char *part_screen)
{
    char tmp[128];
    unsigned char *pic;
    unsigned char *src;
    unsigned char *dst;
    int n, m;

    snprintf(tmp, sizeof(tmp), "graph/%s.pcx", part_screen);
    qdgdfv_clear_virtual_screen();

    pic = qdgdfv_malloc(SCREEN_X_SIZE * SCREEN_Y_SIZE);

    qdgdfv_load_pcx_pal_set(pic, tmp, SCREEN_X_SIZE * SCREEN_Y_SIZE);

    src = pic;
    dst = _qdgdfv_virtual_screen + screen_offset;

    for (n = 0; n < SCREEN_Y_SIZE; n++) {
        for(m = 0; m < SCREEN_X_SIZE; m++) {
            *(dst + m) = *src++;
        }

        dst += _qdgdfv_screen_x_size;
    }

    free(pic);
}


void SwapScreens(int water)
{
    static int _last = -1;
    unsigned char *t;

    if (_last != water) {
        t = _qdgdfv_virtual_screen;
        _qdgdfv_virtual_screen = (unsigned char *) _water_virtual_screen;
        _water_virtual_screen = (char *) t;
        _last = water;
    }
}


void SetupPalettes(void)
{
    int n;

    memcpy(_orig_palette, _qdgdfv_palette, sizeof(_water_palette));
    memcpy(_water_palette, _qdgdfv_palette, sizeof(_water_palette));

    for (n = 0; n < 256 * 3;) {
        _water_palette[n++] /= 3;
        _water_palette[n++] /= 3;
        n++;
    }
}


void SetPalette(int water)
{
    static int _last = 0;

    if (water != _last) {
        memcpy(_qdgdfv_palette, water ? _water_palette : _orig_palette,
               sizeof(_qdgdfv_palette));

        qdgdfv_set_palette();

        _last = water;
    }
}


sp_texture *LoadTexture(char *textfile)
/* Carga una textura. Las texturas soportadas son ficheros PCX de 128x128
   con la paleta normalizada, y se rotan 90? a la izquierda */
{
    int n, m;
    unsigned char c;
    sp_texture *text;
    FILE *f;
    int x, y;

    if ((text = (sp_texture *) SeekCache(textfile)) != NULL)
        return (text);

    text = (sp_texture *) qdgdfv_malloc(TEXTURE_SIZE);

    x = 0;
    y = TEXTURE_Y_SIZE - 1;

    f = qdgdfv_fopen(textfile, "rb");

    /* skip header */
    fseek(f, 128, SEEK_SET);

    n = 0;
    while (n < TEXTURE_SIZE) {
        c = fgetc(f);

        if (c > 0xC0) {
            /* run-length */
            m = c & 0x3F;

            c = fgetc(f);
        }
        else
            m = 1;

        while (m) {
            text[x + y * TEXTURE_X_SIZE] = c;

            y--;

            if (y == -1) {
                x++;
                y = (TEXTURE_Y_SIZE) - 1;
            }

            m--;
            n++;
        }
    }

    fclose(f);

    AddCache(textfile, text);

    return (text);
}


static void AllocVirtualScreens(void)
{
    _water_virtual_screen =
        (char *) qdgdfv_malloc(_qdgdfv_screen_x_size * _qdgdfv_screen_y_size);
}


void WaveWaterTexture(void)
{
    register sp_texture c;
    register sp_texture *optr;
    register sp_texture *dptr;
    int x, y, sx;
    int a, b, fsx;

    optr = _original_water_texture;
    dptr = _water_texture = _wave_water_texture[_wave_water_texture_index];

    for (y = 0; y < TEXTURE_Y_SIZE; y++) {
        a = _water_texture_sin_table[(y + _water_texture_period) & TEXTURE_Y_MASK];

        for (x = 0; x < TEXTURE_X_SIZE; x++) {
            b = _water_texture_cos_table[(x + _water_texture_period)
                             & TEXTURE_X_MASK];

            fsx = (a + b) >> 10;
            sx = (x + fsx) & TEXTURE_X_MASK;

            c = *(optr + sx);
            *dptr = c;
            dptr++;
        }

        optr += TEXTURE_X_SIZE;
    }

    _wave_water_texture_index ^= 1;
    _water_texture_period++;
}


static void FadeEdges(void)
{
    unsigned char *ptr;
    int n, m, s;

    if (!_fade_edges)
        return;

    for (ptr = _qdgdfv_virtual_screen + screen_offset, n = 0; n < SCREEN_Y_SIZE;
        n++, ptr += _qdgdfv_screen_x_size) {
        for (m = 0, s = 14; m < 15; m++, s--) {
            (*(ptr + m)) >>= ((s / 5) + 1);
            (*(ptr + SCREEN_X_SIZE - 1 - m)) >>= ((s / 5) + 1);
        }
    }
}


void DrawWaterVirtualScreen(void)
{
    unsigned char c;
    unsigned char *optr;
    unsigned char *dptr;
    int o;
    int x, y, sx;
    int a, b, fsx;

    optr = (unsigned char *) _water_virtual_screen + screen_offset;
    dptr = _qdgdfv_virtual_screen + screen_offset;

    for (y = 0; y < SCREEN_Y_SIZE; y++) {
        o = (y + _water_screen_period) % SCREEN_Y_SIZE;

        a = _water_screen_sin_table[o];

        for (x = 0; x < SCREEN_X_SIZE; x++) {
            o = (x + _water_screen_period) % _qdgdfv_screen_x_size;

            b = _water_screen_cos_table[o];

            fsx = (a + b) >> 10;
            sx = (x + fsx) % _qdgdfv_screen_x_size;

            c = *(optr + sx);
            *(dptr + x) = c;
        }

        dptr += _qdgdfv_screen_x_size;
        optr += _qdgdfv_screen_x_size;
    }

    _water_screen_period++;

    FadeEdges();

    memset(_qdgdfv_virtual_screen, '\0', _qdgdfv_screen_x_size);
/*
    if(DrawMask!=NULL)
        DrawMask(_water_virtual_screen);*/
}


void InitWater(void)
{
    double f;
    int n;

    for (n = 0; n < TEXTURE_Y_SIZE; n++) {
        f = ((double) n) * 6.283184 / TEXTURE_Y_SIZE;
        f = sin(f) * (1 << 13);
        _water_texture_sin_table[n] = (int) f;
    }

    for (n = 0; n < TEXTURE_X_SIZE; n++) {
        f = ((double) n) * 6.283184 / TEXTURE_X_SIZE;
        f = cos(f) * (1 << 13);
        _water_texture_cos_table[n] = (int) f;
    }

    for (n = 0; n < SCREEN_Y_SIZE; n++) {
        f = ((double) n) * 6.283184 / SCREEN_Y_SIZE;
        f = sin(f) * (1 << 13);
        _water_screen_sin_table[n] = (int) f;
    }

    for (n = 0; n < _qdgdfv_screen_x_size; n++) {
        f = ((double) n) * 6.283184 / _qdgdfv_screen_x_size;
        f = cos(f) * (1 << 13);
        _water_screen_cos_table[n] = (int) f;
    }

    _wave_water_texture[0] = (unsigned char *) qdgdfv_malloc(TEXTURE_SIZE);
    _wave_water_texture[1] = (unsigned char *) qdgdfv_malloc(TEXTURE_SIZE);
}


void GrxStartup(void)
{
/*  strcpy(_qdgdfv_fopen_path, ".;" CONFOPT_PREFIX "/share/splumber");*/

    /* set the application path */
    strcpy(_qdgdfv_fopen_path, ".;");
    strcat(_qdgdfv_fopen_path, qdgdfv_app_dir());
    strcat(_qdgdfv_fopen_path, "splumber");
    strcpy(_qdgdfa_fopen_path, _qdgdfv_fopen_path);

    if (_qdgdfv_scale == 0)
        _qdgdfv_scale = 2;

    _qdgdfa_16_bit = 1;
    _qdgdfv_scale2x = 0;
    _qdgdfv_screen_x_size = 320;
    _qdgdfv_screen_y_size = 200;
    strcpy(_qdgdfv_window_title, "Space Plumber " VERSION);
    strcpy(_qdgdfa_window_title, _qdgdfv_window_title);
    _qdgdfv_use_logger = 0;
    strcpy(_qdgdfv_logger_file, "splumber.log");
    _qdgdfa_sound = 1;

    /* avoid any sound be considered 'big' by QDGDF */
    _qdgdfa_big_sound_threshold = 1024 * 10000;

    qdgdfa_startup();
    qdgdfv_startup();

    qdgdfv_load_ktl_font(_font_file);

    AllocVirtualScreens();
    InitWater();

    screen_offset = ((_qdgdfv_screen_y_size - SCREEN_Y_SIZE) / 2) * _qdgdfv_screen_x_size;
    screen_offset += (_qdgdfv_screen_x_size - SCREEN_X_SIZE) / 2;
}


void GrxShutdown(void)
{
    qdgdfv_shutdown();
    qdgdfa_shutdown();
}
