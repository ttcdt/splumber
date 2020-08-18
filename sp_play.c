/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <time.h>
#include <stdlib.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#include "sp_types.h"
#include "sp_supp.h"
#include "sp_grx.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_play.h"
#include "sp_maze.h"


/* efectos de sonido */

int _snd_left_step;
int _snd_right_step;
int _snd_wleft_step;
int _snd_wright_step;
int _snd_water;
int _snd_underwater;
int _snd_player_dead;
int _snd_pump;
int _snd_console;
int _snd_water_go;


/* ?ltimo segundo contado */
time_t _sp_timer = 0;


/* nivel de sumersi?n del jugador */
/* 0: fuera del agua; 1: con las piernas en el agua; 2: sumergido */
int _player_in_water = 0;

/* contador de movimiento. Se le suma el movimiento que se efect?a,
   y sirve para controlar los pasos */
int _motion_counter = 0;
int _step_toggle = 0;


/* contador de frames */

/* frames por segundo */
int _frames_per_sec = 0;

/* media de frames por segundo */
int _avg_frames_per_sec = 0;

/* frames por segundo, temporal */
int _tmp_frames_per_sec = 0;

/* m?ximos frames por segundo */
int _max_frames_per_sec = 0;

/* flag para mostrar o no los frames por segundo */
int _show_frames_per_sec = 0;

/* flag para deshabilitar la temporizaci?n */
int _no_timing = 0;


/* tiempo de la subida del agua en segundos */
int _water_speed = 0;

/* l?mite de subida del agua */
int _water_limit = 32768;

/* contador de segundos para la subida del agua */
int _seconds_to_water = 1;

/* minutos y segundos de tiempo que quedan */
int _left_minutes = 14;
int _left_seconds = 6;


/* luz rota */
int _broken_light = 0;

/* luz ambiental real */
int _save_ambient_light = 0;

/* segundos para cambiar de estado de rotura */
int _seconds_to_break = 3;


/* ox?geno */
int _oxygen = 1000;

/* segundos bajo el agua */
int _seconds_under_water = 0;

/* jugador muerto */
int _player_dead = 0;

/* el jugador ha ganado */
int _level_resolved = 0;

/* el jugador ha resuelto el ?ltimo nivel */
int _last_level_resolved = 0;

/* no te mueras nunca, maestro */
int _never_die = 0;

/* hace que por defecto se flote */
int _float = 0;

/* hace que por defecto se vaya r?pido */
int _run = 1;


/* colas de sonidos aleatorios */

struct rnd_sound _over_water_sounds;
struct rnd_sound _under_water_sounds;

/* segundos que faltan para el siguiente sonido aleatorio */
int _seconds_to_next_sound = 5;


/* contador de bombas pendientes de activar */
int _num_pumps = 1;

/* contador de consolas pendientes de desactivar */
int _num_consoles = 0;


/* l?neas de mensaje en pantalla */

int _message_secs_left = 0;

char *_msg_0 = NULL;
char *_msg_1 = NULL;
char *_msg_2 = NULL;
char *_msg_3 = NULL;


/* semilla de random */
int _random_seed = -1;

/* nombre del juego con versi?n */
char *_game_name = "Space Plumber " VERSION;

/** temporizador **/

#ifndef CLOCKS_PER_FRAME
#define CLOCKS_PER_FRAME ((CLOCKS_PER_SEC) / 18)
#endif

clock_t _last_clock = 0;

int _user_pause = 0;

/********************
    Code
*********************/

static int MotionFlags(void)
{
    int m = 0;

    if (_qdgdfv_key_up)
        m |= ACT_FORWARD;
    if (_qdgdfv_key_down)
        m |= ACT_BACK;
    if (_qdgdfv_key_left)
        m |= ACT_LEFT;
    if (_qdgdfv_key_right)
        m |= ACT_RIGHT;
    if (_qdgdfv_key_shift_l || _qdgdfv_key_shift_r)
        m |= ACT_FAST;
    if (_qdgdfv_key_space)
        m |= ACT_UP;
    if (_qdgdfv_key_pgup)
        m |= ACT_LOOKUP;
    if (_qdgdfv_key_pgdn)
        m |= ACT_LOOKDOWN;

    return (m);
}


void LoadRandomSound(struct rnd_sound *q, char *sound)
/* carga un sonido en una cola de sonidos aleatorios */
{
    if (q->num == 10)
        return;

    q->sounds[q->num] = qdgdfa_load_sound(sound);
    q->num++;
}


void StartRandomSound(struct rnd_sound *q)
/* lanza un sonido aleatorio */
{
    int pan;
    int s;

    pan = 1 - RANDOM(3);
    s = q->sounds[RANDOM(q->num)];

    qdgdfa_play_sound(s, 0);
    qdgdfa_set_pan(s, pan);
}


void TestSwitch(struct map_block *m, int face, int inc)
/* comprueba la pulsaci?n de pumps ? consolas */
{
    static char tmp[80];
    struct map_face *f;
    sp_object *o;

    /* calcula la cara */
    if (inc > 0)
        face += 2;

    f = m->faces[face];

    if (f == NULL)
        qdgdfv_bang("TestSwitch", "f==NULL");

    o = f->object;

    if (o != NULL) {
        /* se ha encontrado un objeto */
        switch (o->type) {
        case OT_CONSOLE:

            /* consola desactivada */
            _num_consoles--;

            qdgdfa_play_sound(_snd_console, 0);

            /* cambia el gr?fico */
            f->wall = o->t[2];

            sprintf(tmp, "%d CONSOLES LEFT", _num_consoles);

            SetMessage(NULL, "CONSOLE DEACTIVATED", tmp, NULL, 15);

            /* desactiva para siempre */
            o->type = OT_NONE;

            break;

        case OT_PUMP:

            if (_num_consoles != 0) {
                SetMessage(NULL,
                       "CONSOLES STILL ACTIVATED",
                       "DEACTIVATE THEM FIRST", NULL, 15);
            }
            else {
                /* bomba activada */
                _num_pumps--;

                qdgdfa_play_sound(_snd_pump, 0);

                /* cambia el gr?fico */
                f->wall = o->t[1];

                sprintf(tmp, "%d PUMPS LEFT", _num_pumps);

                SetMessage(NULL, "PUMP ACTIVATED", tmp, NULL, 15);

                /* desactiva para siempre */
                o->type = OT_NONE;
            }

            break;
        }
    }
}


void Motion(int *px, int *py, int *pz, int *pa)
/* Mueve las coordenadas seg?n su posici?n en el mapa y
   las acciones en motion */
{
    int inc;
    int x, y, z, a;
    struct map_block *m;
    struct map_block *n;
    int in_water;
    int i_sin, i_cos;
    int incx, incz;
    int move;
    int ps_total;
    static int swim = 0;
    int motion;

    /* F1: high / low detail switch */
    if (qdgdfv_extended_key_status(&_qdgdfv_key_f1) == 1)
        _low_detail ^= 1;

    /* F3: motion blur switch */
    if (qdgdfv_extended_key_status(&_qdgdfv_key_f3) == 1)
        _use_motion_blur ^= 1;

    /* F4: scale2x usage switch */
    if (qdgdfv_extended_key_status(&_qdgdfv_key_f4) == 1)
        _qdgdfv_scale2x ^= 1;

    motion = MotionFlags();

    /* si por defecto se flota y no se est? pulsando la tecla
       de hundirse, se activa la de subir */
    if (_float) {
        /* si se puls? subir, ahora significa bajar */
        if ((motion & ACT_UP))
            motion ^= (ACT_UP | ACT_DOWN);
        else if (!(motion & ACT_DOWN))
            motion |= ACT_UP;
    }

    /* si por defecto hay que correr, se activa */
    if (_run)
        motion ^= ACT_FAST;

    if ((motion & ACT_LOOKUP) && _vertical_bias + PV_LOOK < VERTICAL_BIAS)
        _vertical_bias += PV_LOOK;

    if ((motion & ACT_LOOKDOWN) && _vertical_bias - PV_LOOK > -VERTICAL_BIAS)
        _vertical_bias -= PV_LOOK;

    /* si est? muerto, no se mueve, pero s? cae o se hunde */
    if (_player_dead) {
        motion = 0;
        ps_total = PS_DEAD;
    }
    else
        ps_total = PS_TOTAL;

    x = *px;
    y = *py;
    z = *pz;
    a = *pa;

    /* obtiene el map_block sobre el que est? */
    m = *(m_block + MAP_OFFSET(x >> BLOCK_BITS, z >> BLOCK_BITS));

    if (m == NULL)
        return;

    /* calcula c?mo est? de sumergido */
    inc = y - _water_level;

    if (inc > PS_TOTAL - 1)
        in_water = 0;
    else if (inc > PS_BODY)
        in_water = 1;
    else
        in_water = 2;

    if (y <= _water_level) {
        qdgdfa_stop_sound(_snd_water);
        qdgdfa_play_sound(_snd_underwater, 1);
    }
    else {
        qdgdfa_play_sound(_snd_water, 1);
        qdgdfa_stop_sound(_snd_underwater);

        inc = (y - _water_level) / 3;
        qdgdfa_set_attenuation(_snd_water, inc);
    }

    /* calcula el sonido del agua y?ndose */
    if (_num_pumps == 0) {
        qdgdfa_stop_sound(_snd_water);
        qdgdfa_stop_sound(_snd_underwater);

        qdgdfa_play_sound(_snd_water_go, 1);

        inc = (y - _water_level) / 3;
        qdgdfa_set_attenuation(_snd_water_go, inc);
    }

    _player_in_water = in_water;

    if (in_water) {
        /* si se quiere y se puede nadar hacia arriba, se hace */

        if ((motion & ACT_UP) &&
            y - _water_level <= PS_HEAD && m->ceiling_y - y > PS_HEAD) {
            if (y - _water_level < PS_HEAD)
                y += PV_WATER_UP;

            if (y == _water_level)
                y++;
        }
        else {
            /* se hunde */
            if (y - m->floor_y > ps_total) {
                if (motion & ACT_DOWN)
                    y -= PV_WATER_DOWN;
                else
                    y -= PV_WATER_FALL;
            }

            if (y == _water_level)
                y--;
        }
    }
    else {
        /* fuera del agua */

        if (y - m->floor_y > PS_TOTAL) {
            /* estamos por encima del agua: caer */
            y -= PV_FALL;

            if (y - m->floor_y < PS_TOTAL) {
                /* se ajusta si se ha ca?do demasiado */
                y = m->floor_y + PS_TOTAL;
            }
        }
    }

    /* sube un posible escal?n */
    if (y - m->floor_y < ps_total)
        y += PV_FALL;

    /* giros */
    if (motion & ACT_FAST)
        inc = PV_FAST_TURN;
    else
        inc = PV_TURN;

    if (motion & ACT_LEFT) {
        a += inc;

        if (a >= ANGLE(360))
            a -= ANGLE(360);
    }
    else if (motion & ACT_RIGHT) {
        a -= inc;

        if (a < 0)
            a += ANGLE(360);
    }

    /* calcula el signo del movimiento */
    if (motion & ACT_FORWARD)
        move = 1;
    else if (motion & ACT_BACK)
        move = -1;
    else
        move = 0;

    if (y < _water_level) {
        if (move == 1)
            inc = PV_WATER_FWD;
        else
            inc = PV_WATER_BACK;

        inc = 2 + 2 * (PV_WATER_FWD - ((swim >> 2) % PV_WATER_FWD));
        swim++;
    }
    else if (y < _water_level + PS_BODY) {
        if (move == 1)
            inc = PV_SWIM_FWD;
        else
            inc = PV_SWIM_BACK;

        inc = 2 + 2 * (PV_WATER_FWD - ((swim >> 2) % PV_WATER_FWD));
        swim++;
    }
    else {
        if ((motion & ACT_FAST) && in_water == 0)
            inc = (move == 1 ? PV_FAST_FWD : PV_FAST_BACK);
        else
            inc = (move == 1 ? PV_FORWARD : PV_BACK);
    }

    if (move != 0) {
        /* coge el seno y el coseno */
        SinCos(a, &i_sin, &i_cos);

        incx = (i_cos * inc) >> 11;
        incz = (i_sin * inc) >> 11;

        incx *= move;
        incz *= move;

        /* comprueba si el movimiento es l?cito */

        n = *(m_block + MAP_OFFSET((x + incx * 2) >> BLOCK_BITS, z >> BLOCK_BITS));

        if (n != NULL) {
            if (n->ceiling_y == n->floor_y ||
                n->floor_y - m->floor_y >= PS_LEGS || n->ceiling_y < y) {
                TestSwitch(n, 0, incx);

                incx = 0;
            }
        }

        n = *(m_block + MAP_OFFSET(x >> BLOCK_BITS, (z - incz * 2) >> BLOCK_BITS));

        if (n != NULL) {
            if (n->ceiling_y == n->floor_y ||
                n->floor_y - m->floor_y >= PS_LEGS || n->ceiling_y < y) {
                TestSwitch(n, 1, incz);

                incz = 0;
            }
        }

        x += incx;
        z -= incz;

        if (incx || incz)
            _motion_counter += inc;

        if (_motion_counter > BLOCK_SIZE) {
            _motion_counter = 0;

            if (in_water == 0) {
                if (_step_toggle)
                    qdgdfa_play_sound(_snd_left_step, 0);
                else
                    qdgdfa_play_sound(_snd_right_step, 0);
            }
            else if (in_water == 1) {
                if (_step_toggle)
                    qdgdfa_play_sound(_snd_wleft_step, 0);
                else
                    qdgdfa_play_sound(_snd_wright_step, 0);
            }

            _step_toggle ^= 1;
        }
    }

    *px = x;
    *py = y;
    *pz = z;
    *pa = a;

    if (motion & ACT_MASK)
        _motion_blur = _use_motion_blur;
}


void SetMessage(char *msg_0, char *msg_1, char *msg_2, char *msg_3, int seconds)
/* define las l?neas de mensaje */
{
    _msg_0 = msg_0;
    _msg_1 = msg_1;
    _msg_2 = msg_2;
    _msg_3 = msg_3;

    _message_secs_left = seconds;
}


void PlayerDrawMask(void)
{
    char tmp[40];
    int n;
    unsigned char *ptr;

    /* clear the bottom part of the screen */
    ptr = _qdgdfv_virtual_screen + (_qdgdfv_screen_x_size * _qdgdfv_screen_y_size) - 1;
    for (n = screen_offset + _qdgdfv_screen_x_size; n; n--, ptr--)
        *ptr = _qdgdfv_clear_color;

    /* Stratos, por alguna misteriosa raz?n, no quiere el t?tulo
       del juego en pantalla */
/*  qdgdfv_font_print(-1, 0, (unsigned char *) "SPACE PLUMBER " VERSION, 255);*/

    sprintf(tmp, "OXYGEN: %d%%", _oxygen / 10);
    qdgdfv_font_print(2, -1, (unsigned char *) tmp, 255);

    if (_message_secs_left) {
        int y = (_qdgdfv_screen_y_size / 2) - (_qdgdfv_font_height * 2) - (_qdgdfv_font_height / 2);

        qdgdfv_font_print(-1, y += _qdgdfv_font_height, (unsigned char *) _msg_0, 254);
        qdgdfv_font_print(-1, y += _qdgdfv_font_height, (unsigned char *) _msg_1, 254);
        qdgdfv_font_print(-1, y += _qdgdfv_font_height, (unsigned char *) _msg_2, 254);
        qdgdfv_font_print(-1, y += _qdgdfv_font_height, (unsigned char *) _msg_3, 254);
    }

    if (_show_frames_per_sec)
        sprintf(tmp, "FPS: %d", _frames_per_sec);
    else
        sprintf(tmp, "TIME: %02d:%02d", _left_minutes, _left_seconds);

    qdgdfv_font_print(-2, -1, (unsigned char *) tmp, 255);
}


int DrawNewFrame(void)
/* devuelve no cero si toca dibujar un nuevo frame */
{
    clock_t c;

    if (_no_timing)
        return (1);

    c = clock();

    if (_last_clock == (clock_t) 0 || _last_clock + CLOCKS_PER_FRAME < c) {
        _last_clock = c;
        return (1);
    }

    return (0);
}


void Objects(void)
/* procesa los objetos */
{
    sp_object *o;

    for (o = objects; o != NULL; o = o->next) {
        switch (o->type) {
        case OT_PUMP:

            o->face->wall = o->t[0];

            break;

        case OT_CONSOLE:
        case OT_BLINK:
        case OT_BLINK_LIGHT:

            /* si se pas? el tiempo... */
            if (o->clicks == 0) {
                o->clicks = 1 + RANDOM(20);

                if (o->status) {
                    o->face->wall = o->t[1];
                    o->face->idim = o->idim;
                }
                else {
                    o->face->wall = o->t[0];

                    if (o->type == OT_BLINK_LIGHT)
                        o->face->idim = -127;
                }

                o->status = !o->status;
            }
            else
                o->clicks--;

            break;

        case OT_ANIMATION:
        case OT_SEQUENCE:

            if (o->clicks == 0) {
                o->clicks = o->type == OT_SEQUENCE ? 1 : 9;

                o->status++;

                if (o->status == 4 || o->t[o->status] == NULL)
                    o->status = 0;

                o->face->wall = o->t[o->status];
            }
            else
                o->clicks--;

            break;
        }
    }
}


void TimeEvents(int py)
/* Gestiona los eventos de tiempo */
{
    time_t t;
    static char tmp[40];

    /* si no quedan bombas que activar, el agua baja */
    if (_num_pumps == 0) {
        if (_water_level < py - PS_HEAD && !_player_dead) {
            if (!_level_resolved) {
                if (_level == 0) {
                    SetMessage(NULL,
                           "LEVEL SECURED",
                           "PRESS esc TO CONTINUE", NULL, 40);
                }
                else {
                    _level++;
                    ReadLevel();

                    if (_password == 999999)
                        _last_level_resolved = 1;
                    else {
                        sprintf(tmp, "NEXT LEVEL CODE: %d", _password);

                        SetMessage("LEVEL SECURED",
                               "PRESS esc TO CONTINUE",
                               NULL, tmp, 40);
                    }
                }
            }

            _level_resolved = 1;
        }

        if (_water_level > 0)
            _water_level--;
    }

    t = time(NULL);

    if (_sp_timer == (time_t) NULL || _sp_timer != t) {
        /* controla si toca que el agua suba */

        _seconds_to_water--;

        if (_seconds_to_water <= 0) {
            /* sube */
            if (_water_level < _water_limit)
                _water_level++;

            _seconds_to_water = _water_speed;
        }

        /* cuenta un segundo menos del mensaje en pantalla */

        if (_message_secs_left > 0) {
            _message_secs_left--;

            if (_message_secs_left == 0)
                _msg_0 = _msg_1 = _msg_2 = _msg_3 = NULL;
        }

        /* cuenta atr?s */

        if (!_player_dead && !_level_resolved) {
            _left_seconds--;

            if (_left_seconds < 0) {
                _left_seconds = 59;

                _left_minutes--;

                if (_left_minutes < 0) {
                    _left_minutes = 0;
                    _left_seconds = 0;
                }
            }
        }

        /* comprueba si hay que lanzar un nuevo sonido */
        _seconds_to_next_sound--;

        if (_seconds_to_next_sound <= 0) {
            _seconds_to_next_sound = 5 + RANDOM(10);

            if (py < _water_level)
                StartRandomSound(&_under_water_sounds);
            else
                StartRandomSound(&_over_water_sounds);
        }

        /* refresco de ox?geno */
        if (!_player_dead) {
            if (py < _water_level) {
                if (!_never_die)
                    _oxygen -= 16;

                if (_oxygen <= 0) {
                    _oxygen = 0;
                    _player_dead = 1;

                    qdgdfa_play_sound(_snd_player_dead, 0);

                    SetMessage(NULL,
                           "YOU ARE DEAD",
                           "PRESS esc TO CONTINUE", NULL, 40);
                }
            }
            else {
                _oxygen += 28;

                if (_oxygen > 1000)
                    _oxygen = 1000;
            }
        }

        /* controla la luz rota */
        if (_broken_light) {
            _seconds_to_break--;

            if (_seconds_to_break <= 0) {
                _seconds_to_break = 1 + RANDOM(3);

                if (_save_ambient_light == 0) {
                    /* toca oscurecerse */
                    _save_ambient_light = _ambient_light;

                    _ambient_light -= 2;
                    if (_ambient_light < 5)
                        _ambient_light = 5;
                }
                else {
                    /* toca restablecerse */
                    _ambient_light = _save_ambient_light;
                    _save_ambient_light = 0;
                }
            }
        }

        /* actualiza el conteo de frames */
        _frames_per_sec = _tmp_frames_per_sec;
        _tmp_frames_per_sec = 0;

        if (_max_frames_per_sec < _frames_per_sec)
            _max_frames_per_sec = _frames_per_sec;

        if (_avg_frames_per_sec) {
            _avg_frames_per_sec += _frames_per_sec;
            _avg_frames_per_sec >>= 1;
        }
        else
            _avg_frames_per_sec = _frames_per_sec;

        /* nuevo segundo */
        _sp_timer = t;
    }
    else
        _tmp_frames_per_sec++;
}


void PlayerStartup(void)
{
}


void PlayerShutdown(void)
{
}
