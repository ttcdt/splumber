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

#include "sp_types.h"
#include "sp_supp.h"
#include "sp_grx.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_maze.h"
#include "sp_play.h"

extern int _menu;
extern int _custom;
extern int _intro;

struct int_param {
    char *label;
    int *value;
};

struct string_param {
    char *label;
    char *value;
};


struct int_param int_params[] = {
    {"low_detail", &_low_detail},
    {"use_motion_blur", &_use_motion_blur},
    {"show_frames_per_sec", &_show_frames_per_sec},
    {"ambient_light", &_ambient_light},
    {"water_speed", &_water_speed},
    {"num_pumps", &_num_pumps},
    {"num_consoles", &_num_consoles},
    {"random_seed", &_random_seed},
    {"maze_steps", &_maze_steps},
    {"broken_light", &_broken_light},
    {"area_num", &_area_num},
    {"dump_maze", &_dump_maze},
    {"never_die", &_never_die},
    {"no_timing", &_no_timing},
    {"menu", &_menu},
    {"level", &_level},
    {"custom", &_custom},
    {"wave_water", &_wave_water},
    {"intro", &_intro},
    {"float", &_float},
    {"run", &_run},
    {"perspective_correct", &_perspective_correct},
    {NULL, NULL}
};

struct string_param string_params[] = {
    {"font_file", _font_file},
    {"area_file", _area_file},
    {NULL, NULL}
};


char _main_argv[500] = "";


/********************
    Code
*********************/

void _ParseParam(char *p)
{
    char param[250];
    char *label;
    char *value;
    int ivalue;
    struct int_param *i;
    struct string_param *s;

    strcpy(param, p);

    label = strtok(param, "=");
    value = strtok(NULL, "");

    for (i = int_params; i->label != NULL; i++) {
        if (strcmp(label, i->label) == 0) {
            ivalue = atoi(value);
            *i->value = ivalue;

            return;
        }
    }

    for (s = string_params; s->label != NULL; s++) {
        if (strcmp(label, s->label) == 0) {
            strncpy(s->value, value, 150);

            return;
        }
    }
}


void ParseParam(char *p)
{
    int goon;
    char *s;

    if (p == NULL)
        return;

    s = p;

    goon = 1;
    for (;;) {
        /* search for a , */
        while (*s != '\0' && *s != ',' && *s != ';')
            s++;

        if (*s == '\0')
            goon = 0;
        else
            *s = '\0';

        _ParseParam(p);

        if (!goon)
            break;

        s++;
        p = s;
    }
}


void ParseFile(char *file)
{
    FILE *f;
    char lin[1024];

    if ((f = fopen(file, "r")) == NULL)
        return;

    for (;;) {
        if (fgets(lin, sizeof(lin) - 1, f) == NULL)
            break;

        if (lin[strlen(lin) - 1] == '\n')
            lin[strlen(lin) - 1] = '\0';

        if (lin[strlen(lin) - 1] == '\r')
            lin[strlen(lin) - 1] = '\0';

        if (lin[0] == '\0' || lin[0] == ';' || lin[0] == '#')
            continue;

        ParseParam(lin);
    }

    fclose(f);
}


void ParseMain(int argc, char *argv[])
{
    int n;

    _main_argv[0] = '\0';

    for (n = 1; n < argc; n++) {
        ParseParam(argv[n]);

        if (n != 1)
            strcat(_main_argv, " ");

        strcat(_main_argv, argv[n]);
    }
}
