/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#include "sp_types.h"
#include "sp_supp.h"
#include "sp_grx.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_maze.h"
#include "sp_play.h"

/* estilos */

int n_styles = -1;

struct style styles[MAX_STYLES];

/* nombre del área */
char _area_name[80];

/* fichero que contiene el área */
char _area_file[150] = "etc/a1.def";

/* número que identifica al área */
int _area_num = 0;


/** laberinto **/

static int _inc_x[4] = { +1, 0, -1, 0 };
static int _inc_z[4] = { 0, -1, 0, +1 };

#define DIR(d)  ((d) & 0x3)
#define INCX(d) _inc_x[(d)]
#define INCZ(d) _inc_z[(d)]


/* pila */

#define STACK_ELEMS 1000

/* estructura de un elemento en la pila de segmentos */
struct seg_stack {
    int type;       /* tipo de segmento ST_* */
    int wx;         /* x */
    int wz;         /* z */
    int dir;        /* dirección */
    int floor_y;        /* y del suelo */
    int ceiling_y;      /* y del techo */
    int len;        /* longitud */
    int incy;       /* incremento en las y (escalera) */
    int style;      /* estilo */
    int steps;      /* pasos que se llevan recorridos */
};

/* la pila */
struct seg_stack *s_stack;

/* 'top of stack' */
int s_tos = 0;

/* 'bottom of stack' */
int s_bos = 0;

/* segmentos */

#define ST_LINE     0
#define ST_ROOM     1
#define ST_K        2

#define ST_NUM      ST_K + 1

/* constante especial que hace que alguna característica de
   un segmento sea calculada aleatoriamente */
#define RND -666

/* máximo de pasos a generar en el laberinto */
int _maze_steps = 10;

/* flag para volcar el laberinto a un fichero */
int _dump_maze = 0;

/* bombas y consolas que quedan */
static int _pumps_left;
static int _consoles_left;

/* pasos para bomba, consola y efecto */
static int _steps_to_pump;
static int _steps_to_console;
static int _walls_to_effect;

/*#define RND_PUMP()    (_maze_steps*2)+RANDOM(_maze_steps+1)
*/
#define RND_PUMP()    _maze_steps
#define RND_CONSOLE() RND_PUMP()
#define RND_EFFECT()  3 + RANDOM(6)
#define RND_SPAWN()   1 + RANDOM(len <= 3 ? 1 : len - 3)

/* nivel */
int _level = 1;

/* password del nivel */
int _password;

/********************
    Código
*********************/

void LoadArea(char *areafile)
/* carga un área */
{
    FILE *f;
    char lin[1024];
    char *token;
    char *value;
    int type;

    qdgdfa_reset();

    n_styles = -1;
    _over_water_sounds.num = 0;
    _under_water_sounds.num = 0;

    f = qdgdfv_fopen(areafile, "r");

    for (;;) {
        if (fgets(lin, sizeof(lin) - 1, f) == NULL)
            break;

        if (lin[strlen(lin) - 1] == '\n')
            lin[strlen(lin) - 1] = '\0';

        if (lin[strlen(lin) - 1] == '\r')
            lin[strlen(lin) - 1] = '\0';

        if (lin[0] == '\0' || lin[0] == ';' || lin[0] == '#')
            continue;

        /* comprueba si se entra en el área de estilos */
        if (strcmp(lin, "[style]") == 0) {
            if (n_styles != -1)
                styles[n_styles].n_effects++;

            n_styles++;

            styles[n_styles].n_effects = -1;

            continue;
        }

        /* procesa */

        token = strtok(lin, "= ");
        value = strtok(NULL, "");

        if (n_styles == -1) {
            /* datos preliminares */

            if (strcmp(token, "name") == 0)
                strncpy(_area_name, value, sizeof(_area_name));
            else if (strcmp(token, "palette") == 0) {
                qdgdfv_load_palette(value);
                SetupPalettes();
                SetPalette(1);
                SetPalette(0);
            }
            else if (strcmp(token, "water_texture") == 0) {
                /* textura del agua */

                _original_water_texture = LoadTexture(value);
                WaveWaterTexture();
            }
            else if (strcmp(token, "snd_water") == 0) {
                /* sonido del agua */

                _snd_water = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "snd_underwater") == 0) {
                /* sonido del agua bajo ella */

                _snd_underwater = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "snd_water_go") == 0) {
                /* sonido del agua yéndose */

                _snd_water_go = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "snd_left_step") == 0) {
                _snd_left_step = qdgdfa_load_sound(value);
                qdgdfa_set_pan(_snd_left_step, -1);
            }
            else if (strcmp(token, "snd_right_step") == 0) {
                _snd_right_step = qdgdfa_load_sound(value);
                qdgdfa_set_pan(_snd_right_step, 1);
            }
            else if (strcmp(token, "snd_wleft_step") == 0) {
                _snd_wleft_step = qdgdfa_load_sound(value);
                qdgdfa_set_pan(_snd_wleft_step, -1);
            }
            else if (strcmp(token, "snd_wright_step") == 0) {
                _snd_wright_step = qdgdfa_load_sound(value);
                qdgdfa_set_pan(_snd_wright_step, 1);
            }
            else if (strcmp(token, "snd_player_dead") == 0) {
                _snd_player_dead = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "snd_pump") == 0) {
                _snd_pump = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "snd_console") == 0) {
                _snd_console = qdgdfa_load_sound(value);
            }
            else if (strcmp(token, "over_water_sounds") == 0) {
                LoadRandomSound(&_over_water_sounds, value);
            }
            else if (strcmp(token, "under_water_sounds") == 0) {
                LoadRandomSound(&_under_water_sounds, value);
            }
            else
                qdgdfv_bang("LoadArea", qdgdfv_sprintf("token '%s' ?", token));
        }
        else {
            /* procesando estilos */

            if (strcmp(token, "wall") == 0)
                styles[n_styles].wall = LoadTexture(value);
            else if (strcmp(token, "step") == 0)
                styles[n_styles].step = LoadTexture(value);
            else if (strcmp(token, "floor") == 0)
                styles[n_styles].floor = LoadTexture(value);
            else if (strcmp(token, "ceiling") == 0)
                styles[n_styles].ceiling = LoadTexture(value);
            else if (strcmp(token, "pump1off") == 0)
                styles[n_styles].pump1off = LoadTexture(value);
            else if (strcmp(token, "pump1on") == 0)
                styles[n_styles].pump1on = LoadTexture(value);
            else if (strcmp(token, "pump2off") == 0)
                styles[n_styles].pump2off = LoadTexture(value);
            else if (strcmp(token, "pump2on") == 0)
                styles[n_styles].pump2on = LoadTexture(value);
            else if (strcmp(token, "console1") == 0)
                styles[n_styles].console1 = LoadTexture(value);
            else if (strcmp(token, "console2") == 0)
                styles[n_styles].console2 = LoadTexture(value);
            else if (strcmp(token, "consoleoff") == 0)
                styles[n_styles].consoleoff = LoadTexture(value);
            else if (strcmp(token, "effect") == 0) {
                /* nuevo efecto */

                styles[n_styles].n_effects++;

                type = ET_NONE;

                if (strcmp(value, "static") == 0)
                    type = ET_STATIC;
                else if (strcmp(value, "blink") == 0)
                    type = ET_BLINK;
                else if (strcmp(value, "blink_light") == 0)
                    type = ET_BLINK_LIGHT;
                else if (strcmp(value, "lit") == 0)
                    type = ET_LIT;
                else if (strcmp(value, "animation") == 0)
                    type = ET_ANIMATION;
                else if (strcmp(value, "sequence") == 0)
                    type = ET_SEQUENCE;
                else {
                    qdgdfv_bang("LoadArea",
                            qdgdfv_sprintf("effect '%s' ?", value));
                }

                styles[n_styles].effects[styles[n_styles].n_effects].type =
                    type;
            }
            else if (strcmp(token, "texture1") == 0)
                styles[n_styles].effects[styles[n_styles].n_effects].t[0] =
                    LoadTexture(value);
            else if (strcmp(token, "texture2") == 0)
                styles[n_styles].effects[styles[n_styles].n_effects].t[1] =
                    LoadTexture(value);
            else if (strcmp(token, "texture3") == 0)
                styles[n_styles].effects[styles[n_styles].n_effects].t[2] =
                    LoadTexture(value);
            else if (strcmp(token, "texture4") == 0)
                styles[n_styles].effects[styles[n_styles].n_effects].t[3] =
                    LoadTexture(value);
            else
                qdgdfv_bang("LoadArea",
                        qdgdfv_sprintf("[style] token '%s' ?", token));
        }
    }

    fclose(f);

    if (n_styles != -1)
        styles[n_styles].n_effects++;

    n_styles++;

    qdgdfv_logger("LoadArea", _area_file);
}



/** laberinto aleatorio **/

int BuildFloor(int wx, int wz, int style, int floor_y, int ceiling_y)
/* crea un suelo */
{
    int n;
    struct map_block *m;
    struct map_face *f;

    /* si ya hay algo ahí, no vale */
    if (*(m_block + MAP_OFFSET(wx, wz)) != NULL) {
        qdgdfv_bang("BuildFloor", "*m!=NULL");
        return (0);
    }

    /* crea el map_block */
    m = AllocMapBlock();

    /* alturas de suelo y techo */
    m->floor_y = floor_y;
    m->ceiling_y = ceiling_y;

    /* texturas */
    m->ceiling = styles[style].ceiling;
    m->floor = styles[style].floor;

    /* las 4 caras (escalones y dinteles) */
    for (n = 0; n < 4; n++) {
        f = AllocMapFace();
        f->idim = 0;
        f->wall_size = f->step_size = 0;
        f->wall = f->step = styles[style].step;

        m->faces[n] = f;
    }

    *(m_block + MAP_OFFSET(wx, wz)) = m;

    /* hace que el nivel del agua sea el del suelo más bajo */
    if (_water_level > floor_y)
        _water_level = floor_y;

    /* coge el techo más alto */
    if (_water_limit < ceiling_y)
        _water_limit = ceiling_y;

    return (1);
}


int BuildWall(int wx, int wz, int style, int dir, int floor_y, int steps)
/* construye la cara visible desde la dirección dir en wx y wz */
{
    struct map_block *m;
    struct map_face *f;
    int face;
    int effect;
    sp_object *o;
    int n;
    sp_texture *t;
    int pump12 = 0;

    dir = DIR(dir);
    face = DIR(dir + 2);

    /* avanza en la dirección dir */
    wx += INCX(dir);
    wz += INCZ(dir);

    /* si ya hay algo ahí, se intenta usar */
    m = *(m_block + MAP_OFFSET(wx, wz));

    if (m == NULL) {
        m = AllocMapBlock();
        *(m_block + MAP_OFFSET(wx, wz)) = m;
        m->ceiling_y = m->floor_y = floor_y;
    }
    else if (m->floor_y > floor_y) {
        /* si el bloque ya existe (por tanto, ya hay una cara
           definida), sólo definir la altura si ésta nueva es
           más baja, para evitar que la otra cara se quede corta */
        m->ceiling_y = m->floor_y = floor_y;
    }

    m->ceiling = m->floor = NULL;

    f = AllocMapFace();
    f->idim = 0;
    f->wall = styles[style].wall;
    f->step = NULL;

    /* efectos */

    _walls_to_effect--;

    if (_walls_to_effect <= 0 && styles[style].n_effects) {
        _walls_to_effect = RND_EFFECT();

        effect = RANDOM(styles[style].n_effects);

        switch (styles[style].effects[effect].type) {
        case ET_STATIC:
            f->wall = styles[style].effects[effect].t[0];
            break;

        case ET_LIT:
            f->wall = styles[style].effects[effect].t[0];
            f->idim = -127;
            break;

        case ET_BLINK:
        case ET_BLINK_LIGHT:
        case ET_ANIMATION:
        case ET_SEQUENCE:

            o = AllocObject(f);
            o->type = styles[style].effects[effect].type;

            for (n = 0; n < 4; n++)
                o->t[n] = styles[style].effects[effect].t[n];

            break;
        }
    }
    else if (_steps_to_console == steps && _consoles_left) {
        if (styles[style].console1 != NULL) {
/*            _steps_to_pump+=RND_PUMP();
            _steps_to_console+=RND_CONSOLE();
*/
            _steps_to_console--;

            o = AllocObject(f);
            o->type = OT_CONSOLE;
            o->t[0] = styles[style].console1;
            o->t[1] = styles[style].console2;
            o->t[2] = styles[style].consoleoff;

            _consoles_left--;
        }
    }
    else if (_steps_to_pump == steps && _pumps_left) {
        if (pump12 & 1)
            t = styles[style].pump1off;
        else
            t = styles[style].pump2off;

        if (t != NULL) {
/*            _steps_to_pump+=RND_PUMP();
            _steps_to_console+=RND_CONSOLE();
*/
            _steps_to_pump--;
            _steps_to_console = _steps_to_pump - 2;

            o = AllocObject(f);
            o->type = OT_PUMP;

            if (pump12 & 1) {
                o->t[0] = styles[style].pump1off;
                o->t[1] = styles[style].pump1on;
            }
            else {
                o->t[0] = styles[style].pump2off;
                o->t[1] = styles[style].pump2on;
            }

            _pumps_left--;
        }

        pump12++;
    }

    m->faces[face] = f;

    return (1);
}


void DumpMaze(void)
/* vuelca el laberinto a texto */
{
    FILE *f;
    int x, z;
    struct map_block *m;
    struct map_face *mf;
    sp_object *o;
    int n;
    int c;

    f = fopen("/tmp/smaze.txt", "w");

    for (z = 1; z < 255; z++) {
        for (x = 1; x < 255; x++) {
            m = *(m_block + MAP_OFFSET(x, z));

            if (m == NULL)
                fprintf(f, " ");
            else if (m->ceiling_y == m->floor_y) {
                c = '#';

                for (n = 0; n < 4 && c == '#'; n++) {
                    mf = m->faces[n];

                    if (mf != NULL) {
                        o = mf->object;

                        if (o != NULL) {
                            if (o->type == OT_PUMP)
                                c = 'P';
                            if (o->type == OT_CONSOLE)
                                c = 'C';
                        }
                    }
                }

                fprintf(f, "%c", c);
            }
            else {
                if (x == 128 && z == 128)
                    fprintf(f, "0");
                else
                    fprintf(f, "+");
            }
        }

        fprintf(f, "\n");
    }

    fprintf(f, "\nrandom_seed=%d\n", _random_seed);

    fclose(f);
}


void PushSegment(int type, int wx, int wz, int dir, int floor_y,
         int ceiling_y, int len, int incy, int style, int steps)
/* almacena un segmento en la pila */
{
    struct seg_stack *s;

    /* ajusta */
    dir = DIR(dir);

    /* valores aleatorios */

    if (type == RND)
        type = RANDOM(ST_NUM);

    if (len == RND)
        len = 5 + RANDOM(4);

    if (incy == RND) {
        incy = 22 - RANDOM(44);

        if (incy > -7 && incy < 7)
            incy = 0;

        if (incy > 16)
            incy = 16;
        if (incy < -16)
            incy = -16;
    }

    if (style == RND)
        style = RANDOM(n_styles);


    /* si la pila está llena, cierra */
    if (s_tos == STACK_ELEMS) {
        qdgdfv_logger("PushSegment", "out of stack");
        BuildWall(wx, wz, style, dir, floor_y, steps);
    }
    else {
        s = &s_stack[s_tos];

        s->type = type;
        s->wx = wx;
        s->wz = wz;
        s->dir = dir;
        s->floor_y = floor_y;
        s->ceiling_y = ceiling_y;
        s->len = len;
        s->incy = incy;
        s->style = style;
        s->steps = steps;

        s_tos++;
    }
}

int PopSegment(int *type, int *wx, int *wz, int *dir, int *floor_y,
           int *ceiling_y, int *len, int *incy, int *style, int *steps)
/* recupera un segmento de la pila */
{
    struct seg_stack *s;

    /* devuelve 0 si se ha acabado la pila */
/*    if(s_tos==0)
        return(0);

    s_tos--;

    s=&s_stack[s_tos];
*/
    if (s_bos == s_tos)
        return (0);
    s = &s_stack[s_bos];
    s_bos++;

    *type = s->type;
    *wx = s->wx;
    *wz = s->wz;
    *dir = s->dir;
    *floor_y = s->floor_y;
    *ceiling_y = s->ceiling_y;
    *len = s->len;
    *incy = s->incy;
    *style = s->style;
    *steps = s->steps;

    return (1);
}


int TestLine(int wx, int wz, int dir, int len)
/* devuelve la máxima longitud que puede tener un segmento en esa dir */
{
    int n;
    struct map_block *m;

    len++;

    wx += INCX(dir);
    wz += INCZ(dir);

    for (n = 0; n < len; n++) {
        wx += INCX(dir);
        wz += INCZ(dir);

        m = *(m_block + MAP_OFFSET(wx, wz));

        if (m != NULL)
            break;
    }

    if (n == len)
        return (n - 1);
    else
        return (n);
}


int TestBlock(int wx, int wz, int dir, int len, int left, int right)
/* comprueba que un bloque esté libre */
{
    int n;
    int incx, incz;

    /* línea central */
    len = TestLine(wx, wz, dir, len);

    /* a la izquierda */
    incx = INCX(DIR(dir + 1));
    incz = INCZ(DIR(dir + 1));

    for (n = 1; n <= left; n++)
        len = TestLine(wx + (incx * n), wz + (incz * n), dir, len);

    /* a la derecha */
    incx = INCX(DIR(dir - 1));
    incz = INCZ(DIR(dir - 1));

    for (n = 1; n <= right; n++)
        len = TestLine(wx + (incx * n), wz + (incz * n), dir, len);

    return (len);
}


/** Segmentos **/

int SpawnLineSegment(int wx, int wz, int dir, int floor_y, int ceiling_y,
             int len, int incy, int style, int steps)
/* traza una línea */
{
    int n;
    int spawn_left;
    int spawn_right;

    /* calcula la disponibilidad del espacio */

    len = TestBlock(wx, wz, dir, len, 1, 1);

    if (len <= 0)
        return (0);

    /* bloques */

    /* el 2+ hace que haya siempre 2 muros de separación */
    spawn_left = RND_SPAWN();
    spawn_right = RND_SPAWN();

    for (n = 0; n < len; n++) {
        /* avanza */
        wx += INCX(dir);
        wz += INCZ(dir);

        steps++;

        /* suelo */
        BuildFloor(wx, wz, style, floor_y, ceiling_y);

        /* muro o nuevo segmento a la izquierda */
        if (spawn_left <= 0 && n < len - 2) {
            spawn_left = RND_SPAWN();

            PushSegment(RND, wx, wz, dir + 1,
                    floor_y, ceiling_y, RND, RND, RND, steps);
        }
        else
            BuildWall(wx, wz, style, dir + 1, floor_y, steps);

        /* muro o nuevo segmento a la derecha */
        if (spawn_right <= 0 && n < len - 2) {
            spawn_right = RND_SPAWN();

            PushSegment(RND, wx, wz, dir - 1,
                    floor_y, ceiling_y, RND, RND, RND, steps);
        }
        else
            BuildWall(wx, wz, style, dir - 1, floor_y, steps);

        /* incrementa */
        floor_y += incy;
        ceiling_y += incy;

        spawn_left--;
        spawn_right--;
    }

    floor_y -= incy;
    ceiling_y -= incy;

    /* cierra */
    BuildWall(wx, wz, style, dir, floor_y, steps);

    return (1);
}


int SpawnRoomSegment(int wx, int wz, int dir, int floor_y, int ceiling_y,
             int len, int incy, int style, int steps)
/* crea una habitación */
{
    int n;

    /* controla si cabe el pasillo */
    if (TestBlock(wx, wz, dir, 2, 1, 1) != 2)
        return (0);

    /* pasillo inicial */
    for (n = 0; n < 2; n++) {
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);

        BuildFloor(wx, wz, style, floor_y, ceiling_y);
        BuildWall(wx, wz, style, dir + 1, floor_y, steps);
        BuildWall(wx, wz, style, dir - 1, floor_y, steps);
    }

    /* controla si cabe la habitación */
    if (TestBlock(wx, wz, dir, 4, 2, 2) != 4) {
        BuildWall(wx, wz, style, dir, floor_y, steps);
        return (1);
    }

    steps++;
    wx += INCX(dir);
    wz += INCZ(dir);

    /* crea el suelo de entrada y el central */
    BuildFloor(wx, wz, style, floor_y, ceiling_y);

    if (incy >= 0)
        BuildFloor(wx + INCX(dir), wz + INCZ(dir), style, floor_y, ceiling_y);
    else
        BuildWall(wx, wz, style, dir, floor_y, steps);

    /* gira a la izquierda */
    dir = DIR(dir + 1);

    /* esto lo hace 3 veces */
    for (n = 0; n < 3; n++) {
        /* esquina */
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);

        BuildFloor(wx, wz, style, floor_y, ceiling_y);
        BuildWall(wx, wz, style, dir + 1, floor_y, steps);
        BuildWall(wx, wz, style, dir, floor_y, steps);

        /* gira a la derecha */
        dir = DIR(dir - 1);

        /* crea un suelo */
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);
        BuildFloor(wx, wz, style, floor_y, ceiling_y);

        if (incy < 0)
            BuildWall(wx, wz, style, dir - 1, floor_y, steps);

        /* un nuevo pasillo */
        PushSegment(RND, wx, wz, dir + 1, floor_y, ceiling_y, RND, RND, RND, steps);
    }

    /* última esquina */
    steps++;
    wx += INCX(dir);
    wz += INCZ(dir);

    BuildFloor(wx, wz, style, floor_y, ceiling_y);
    BuildWall(wx, wz, style, dir + 1, floor_y, steps);
    BuildWall(wx, wz, style, dir, floor_y, steps);

    return (1);
}


int SpawnKSegment(int wx, int wz, int dir, int floor_y, int ceiling_y,
          int len, int incy, int style, int steps)
/* crea una bifurcación en forma de K */
{
    int n;
    int or;

    /* el signo de incy da la orientación */
    if (incy < 0)
        or = -1;
    else
        or = 1;

    /* controla si cabe el pasillo */
    if (TestBlock(wx, wz, dir, 2, 1, 1) != 2)
        return (0);

    /* pasillo inicial */
    for (n = 0; n < 2; n++) {
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);

        BuildFloor(wx, wz, style, floor_y, ceiling_y);
        BuildWall(wx, wz, style, dir + 1, floor_y, steps);
        BuildWall(wx, wz, style, dir - 1, floor_y, steps);
    }

    /* controla si cabe la habitación */
    if (TestBlock(wx, wz, dir, 4, 1, 1) != 4) {
        BuildWall(wx, wz, style, dir, floor_y, steps);
        return (1);
    }

    /* bloques sin bifurcación */
    for (n = 0; n < 4; n++) {
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);

        BuildFloor(wx, wz, style, floor_y, ceiling_y);
        BuildWall(wx, wz, style, dir + or, floor_y, steps);
    }

    /* un bloque hacia adelante */
    PushSegment(RND, wx, wz, dir, floor_y, ceiling_y, RND, RND, RND, steps);

    /* gira  */
    dir = DIR(dir - or);

    steps++;
    wx += INCX(dir);
    wz += INCZ(dir);

    BuildFloor(wx, wz, style, floor_y, ceiling_y);
    BuildWall(wx, wz, style, dir + or, floor_y, steps);

    /* vuelve a girar */
    dir = DIR(dir - or);

    /* un segmento */
    PushSegment(ST_LINE, wx, wz, dir + or, floor_y, ceiling_y, RND, RND, RND, steps);

    /* muro entre las dos bifurcaciones */
    for (n = 0; n < 2; n++) {
        steps++;
        wx += INCX(dir);
        wz += INCZ(dir);

        BuildFloor(wx, wz, style, floor_y, ceiling_y);
        BuildWall(wx, wz, style, dir + or, floor_y, steps);
    }

    /* segunda bifurcación */
    steps++;
    wx += INCX(dir);
    wz += INCZ(dir);

    BuildFloor(wx, wz, style, floor_y, ceiling_y);

    /* un segmento */
    PushSegment(ST_LINE, wx, wz, dir + or, floor_y, ceiling_y, RND, RND, RND, steps);

    /* muro enfrente */
    BuildWall(wx, wz, style, dir, floor_y, steps);

    return (1);
}


void SpawnMaze(void)
/* traza el laberinto */
{
    int type;
    int wx;
    int wz;
    int dir;
    int floor_y;
    int ceiling_y;
    int len;
    int incy;
    int style;
    int steps;

    int ret;

    for (;;) {
        if (!PopSegment(&type, &wx, &wz, &dir, &floor_y, &ceiling_y,
                &len, &incy, &style, &steps))
            break;

        if (steps > _maze_steps)
            ret = 0;
        else {
            switch (type) {
            case ST_LINE:
                ret = SpawnLineSegment(wx, wz, dir, floor_y, ceiling_y,
                               len, incy, style, steps);

                break;

            case ST_ROOM:
                ret = SpawnRoomSegment(wx, wz, dir, floor_y, ceiling_y,
                               len, incy, style, steps);

                break;

            case ST_K:
                ret = SpawnKSegment(wx, wz, dir, floor_y, ceiling_y,
                            len, incy, style, steps);

                break;

            default:
                ret = 0;
            }
        }

        /* si no se ha trazado nada, se cierra */
        if (ret == 0)
            BuildWall(wx, wz, style, dir, floor_y, steps);
    }
}


void InitMaze(void)
/* genera el laberinto aleatorio */
{
    int t;
    int wx, wz, floor_y, ceiling_y;
    int s_size;
    int n, dir;

    RANDOMIZE(_random_seed);

    /* pide el stack */
    s_size = sizeof(struct seg_stack) * STACK_ELEMS;

    s_stack = (struct seg_stack *) qdgdfv_malloc(s_size);
    s_tos = s_bos = 0;

    qdgdfv_logger("InitMaze", qdgdfv_sprintf("%d bytes stack size", s_size));

    /* valores iniciales */
    wx = 128;
    wz = 128;
    floor_y = 16384 - 64;
    ceiling_y = 16384 + 64;

    /* valores aleatorios */
    _steps_to_pump = RND_PUMP();
    _walls_to_effect = RND_EFFECT();
    _steps_to_console = _steps_to_pump + 1;

    /* el agua arriba del todo, para detectar el valor más bajo */
    _water_level = _water_limit;
    _water_limit = 0;

    _consoles_left = _num_consoles;
    _pumps_left = _num_pumps;

    /* construye la posición de inicio */
    BuildFloor(wx, wz, 0, floor_y, ceiling_y);

    /* lanza hacia las 4 direcciones */
    dir = RANDOM(4);

    for (n = 0; n < 4; n++)
        PushSegment(ST_LINE, wx, wz, DIR(dir + n),
                floor_y, ceiling_y, RND, RND, RND, 0);

    /* lo traza */
    SpawnMaze();

    /* sube (un poco) el agua */
    _water_level += 32;
    _water_limit -= 32;

    if (_dump_maze)
        DumpMaze();

    if (_pumps_left || _consoles_left)
        qdgdfv_bang("InitMaze", "_pumps_left || _consoles_left");

    /* calcula el tiempo que queda */
    t = (_water_limit - _water_level) * (_water_speed + 1);

    _left_minutes = t / 60;
    _left_seconds = t % 60;

    /* libera el stack */
    free(s_stack);

    OptimizeMap();
}


char *ReadLevel(void)
/* lee la información del nivel definido en _level */
{
    static char level_line[1024];
    FILE *f;
    int n;
    char *ptr;

    /* si no se ha definido un nivel, fuera */
    if (_level == 0)
        return (NULL);

    /* el formato de levels.def es

       PASSWORD:parámetros\r\n

     */
    f = qdgdfv_fopen("etc/levels.def", "r");

    for (n = 0; n < _level; n++) {
        if (fgets(level_line, sizeof(level_line), f) == NULL)
            break;
    }

    /* si se ha llegado hasta ahí */
    if (n == _level) {
        ptr = strtok(level_line, ":");

        if (ptr != NULL) {
            /* obtiene la password */
            _password = atoi(ptr);

            /* coge en ptr el resto de la línea */
            ptr = strtok(NULL, "");
        }
    }
    else
        qdgdfv_bang("ReadLevel", qdgdfv_sprintf("Level %d not found", _level));

    fclose(f);

    return (ptr);
}


void MazeStartup(void)
{
    memset(&styles, '\0', sizeof(styles));

    qdgdfv_clear_virtual_screen();
    qdgdfv_font_print(-1, _qdgdfv_screen_y_size / 2, (unsigned char *) "LOADING...", 254);
    qdgdfv_dump_virtual_screen();

    /* si se ha definido area_num, se rellena _area_file */
    if (_area_num != 0)
        sprintf(_area_file, "etc/a%d.def", _area_num);

    LoadArea(_area_file);

    InitMaze();
}
