/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#include "sp_supp.h"
#include "sp_types.h"
#include "sp_grx.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_maze.h"
#include "sp_play.h"
#include "sp_param.h"

/*
#include "sp_grx.h"
#include "sp_sb.h"
#include "sp_kbd.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_play.h"
#include "sp_maze.h"
#include "sp_param.h"
*/


/* mostrar men? s?/no */
int _menu = 1;

/* laberinto personalizado s?/no */
int _custom = 0;

/* mostrar intro s?/no */
int _intro = 1;

/* m?scara del men? */
unsigned char *_menu_mask = NULL;

/* cursor */
#define CURSOR_SIZE 32 * 32
char cursor[32][32];

/* posiciones x,y del cursor */
int cursor_pos_x[5] = { 143, 58, 89, 160, 171 };
int cursor_pos_y[5] = { 16, 49, 83, 117, 151 };

/* buffer de texto para DrawText */
char _text_mask[80 * 25];

/* sonidos para la m?sica y el agua */
int music;
int water;

int _text_mask_x = -1;

int _tty_under_water = 0;


/********************
    C?digo
*********************/

void Draw_Text(void)
{
    int n, y;
    char line[80];
    char *ptr;

    ptr = _text_mask;

    for (y = 1; y < 25; y++) {
        for (n = 0; *ptr != '\n' && *ptr != '\0' && n < sizeof(line); n++, ptr++)
            line[n] = *ptr;
        line[n] = '\0';

        qdgdfv_font_print(_text_mask_x,
                  y * _qdgdfv_font_height, (unsigned char *) line,
                  y != 1 && _text_mask_x == -1 ? 199 : 255);

        if (*ptr == '\0')
            break;
        else
            ptr++;
    }
}


/** menu **/


void DrawMenu(int menu_opt)
{
    int n, m;
    unsigned char *org;
    unsigned char *des;
    int xcursor, ycursor;

    /* draw the mask over the virtual screen */
    org = _menu_mask;
    des = _qdgdfv_virtual_screen + screen_offset;

    for (n = 0; n < SCREEN_Y_SIZE; n++) {
        for (m = 0; m < SCREEN_X_SIZE; m++) {
            *(des + m) = *org++;
        }

        des += _qdgdfv_screen_x_size;
    }

    /* draw cursor */
    xcursor = cursor_pos_x[menu_opt];
    ycursor = cursor_pos_y[menu_opt];

    des = _qdgdfv_virtual_screen + xcursor + (ycursor * _qdgdfv_screen_x_size) + screen_offset;

    for (n = 0; n < 32; n++) {
        for (m = 0; m < 32; m++) {
            if (cursor[n][m])
                *(des + m) = cursor[n][m];
        }

        des += _qdgdfv_screen_x_size;
    }
}

int Tty(char *message)
{
    clock_t c;
    time_t t;
    char *ptr;

    _text_mask[0] = '\0';
    ptr = _text_mask;

    t = 0;
    _text_mask_x = 8;

    for (;;) {
        c = clock();
        while (c == clock());

        if (_tty_under_water)
            DrawWaterVirtualScreen();

        Draw_Text();
        qdgdfv_dump_virtual_screen();

        qdgdfv_input_poll();

        if (_qdgdfv_key_escape) {
            while (_qdgdfv_key_escape)
                qdgdfv_input_poll();

            return (0);
        }

        if (_qdgdfv_key_enter) {
            while (_qdgdfv_key_enter)
                qdgdfv_input_poll();

            return (1);
        }

        if (t == 0) {
            if (*message == '\a' || *message == '\0')
                t = time(NULL) + 2;
            else {
                *ptr = *message;
                message++;
                ptr++;
                *ptr = '\0';
            }
        }
        else if (time(NULL) > t) {
            t = 0;

            if (*message == '\0')
                break;
            else
                message++;
        }
    }

    qdgdfv_clear_virtual_screen();

    t = time(NULL) + 3;
    while (time(NULL) < t) {
        if (_tty_under_water)
            DrawWaterVirtualScreen();

        qdgdfv_dump_virtual_screen();
    }

    return (1);
}


void KbdGetNumber(char *str, int max)
{
    int n;
    char str_num[15];
    int val;

    qdgdfv_input_poll();

    if (_qdgdfv_alnum >= '0' || _qdgdfv_alnum <= '9') {
        n = _qdgdfv_alnum - '0';

        val = (atoi(str) * 10) + n;

        if (val <= max) {
            str_num[0] = _qdgdfv_alnum;
            str_num[1] = '\0';
            strcat(str, str_num);
        }

        while (_qdgdfv_key_alnum) {
            DrawWaterVirtualScreen();
            Draw_Text();
            qdgdfv_dump_virtual_screen();

            qdgdfv_input_poll();
        }
        _qdgdfv_alnum = '\0';
    }
}


int GetNumber(char *strmask, int max)
{
    char str_num[30];

    str_num[0] = '\0';
    for (;;) {
        sprintf(_text_mask, strmask, str_num);

        DrawWaterVirtualScreen();
        Draw_Text();
        qdgdfv_dump_virtual_screen();

        KbdGetNumber(str_num, max);

        if (_qdgdfv_key_enter) {
            while (_qdgdfv_key_enter)
                qdgdfv_input_poll();

            break;
        }
    }

    return (atoi(str_num));
}


void Intro(void)
/* intro */
{
    time_t t;

    music = qdgdfa_load_sound("sound/holst.wav");
    qdgdfa_play_sound(music, 1);

    LoadScreen("aolabs");

    for (t = time(NULL); t + 10 > time(NULL);) {
        qdgdfv_dump_virtual_screen();

        qdgdfv_input_poll();

        if (_qdgdfv_key_escape) {
            while (_qdgdfv_key_escape)
                qdgdfv_input_poll();

            return;
        }

        if (_qdgdfv_key_enter) {
            while (_qdgdfv_key_enter)
                qdgdfv_input_poll();

            return;
        }
    }

    qdgdfv_clear_virtual_screen();
    qdgdfv_dump_virtual_screen();

    SwapScreens(1);
    LoadScreen("logo");
    SwapScreens(0);

    for (t = time(NULL); t + 15 > time(NULL);) {
        DrawWaterVirtualScreen();
        qdgdfv_dump_virtual_screen();

        qdgdfv_input_poll();

        if (_qdgdfv_key_escape) {
            while (_qdgdfv_key_escape)
                qdgdfv_input_poll();

            return;
        }

        if (_qdgdfv_key_enter) {
            while (_qdgdfv_key_enter)
                qdgdfv_input_poll();

            return;
        }
    }

    LoadScreen("planet");

    if (!Tty
        ("SPACE PLUMBER MISSION BRIEFING\n\n\nPLANET: X-239-LQ (TSUNAMI XXVI)\a\nSURFACE: 100% WATER\a\n"
         "RESOURCES: URANIUM AND THALIUM\a\nEXPLOITED BY:\n"
         "  98% AQUA ENTERPRISES, INC.\n  02% OTHER COMPANIES.\n"))
        return;

    LoadScreen("planet");

    Tty("PROBLEM DESCRIPTION:\a\n  MAIN PROCESSING PLANT\n"
        "  PRESSURE SYSTEM MALFUNCTION.\a\n"
        "  ALL TECHNICIANS EVACUATED.\a\n\n"
        "MISSION: ACTIVATE ALL WATER\n"
        "  EXTRACTION PUMPS.\a\n"
        "  DEACTIVATE SECURITY CONSOLES\n" "  WHERE AVAILABLE.\n");
}


int Menu(void)
{
    int up, down;
    int menu_opt = 0;

    if (_menu_mask == NULL)
        _menu_mask = qdgdfv_malloc(SCREEN_X_SIZE * SCREEN_Y_SIZE);

    _text_mask_x = -1;

    up = down = 0;

    qdgdfv_clear_virtual_screen();
    qdgdfv_load_pcx((unsigned char *) _menu_mask, "graph/menu.pcx", SCREEN_X_SIZE * SCREEN_Y_SIZE);

    SwapScreens(1);
    LoadScreen("wmenu");
    SwapScreens(0);

    qdgdfv_load_pcx((unsigned char *) cursor, "graph/cursor.pcx", CURSOR_SIZE);

    for (;;) {
        DrawWaterVirtualScreen();
        DrawMenu(menu_opt);
        qdgdfv_dump_virtual_screen();

        qdgdfv_input_poll();

        if (_qdgdfv_key_up) {
            down = 0;

            if (up == 0) {
                up = 1;

                menu_opt--;

                if (menu_opt == -1)
                    menu_opt = 4;
            }
        }
        else
            up = 0;

        if (_qdgdfv_key_down) {
            up = 0;

            if (down == 0) {
                down = 1;

                menu_opt++;

                if (menu_opt == 5)
                    menu_opt = 0;
            }
        }
        else
            down = 0;

        if (_qdgdfv_key_escape) {
            menu_opt = -1;
            break;
        }

        if (_qdgdfv_key_enter)
            break;
    }

    /* wait until no key is pressed */
    while (_qdgdfv_key_escape || _qdgdfv_key_enter || _qdgdfv_key_up || _qdgdfv_key_down)
        qdgdfv_input_poll();

    return (menu_opt);
}


void GameEnd(void)
{
    _tty_under_water = 1;
    _fade_edges = 1;

    SwapScreens(1);
    LoadScreen("end");
    SwapScreens(0);

    Tty("SPACE PLUMBER MISSION REPORT\a\n\n"
        "ALL LEVELS HAS BEEN SECURED.\a\n"
        "PRESSURE SYSTEM RESTARTED.\a\n"
        "AQUA ENTERPRISES, INC. MAIN\n"
        "PLANT IS AGAIN FULLY FUNCTIONAL.\a\n\n" "WHAT ABOUT SOME DAYS OF VACATION?\n");

    qdgdfv_clear_virtual_screen();

    Tty("RESPONSE FROM HIGH COMMAND\a\n\n"
        "CONGRATULATIONS.\a\n\n"
        "VACATION REQUESTS MUST BE MADE\n"
        "BY FILLING THE FORM TT-243/F.\a\n"
        "THIS FORM IS UNAVAILABLE FOR NOW.\a\n\n" "END OF TRANSMISSION.\n");

    _level_resolved = 0;
    _last_level_resolved = 0;
}


void GameCore(int x, int y, int z, int a)
/* n?cleo del juego */
{
    int cycle_count = 0;

    while (!_qdgdfv_key_escape && !_last_level_resolved) {
        qdgdfv_input_poll();

        /* F10: pause switch */
        if (qdgdfv_extended_key_status(&_qdgdfv_key_f10) == 1)
            _user_pause ^= 1;

        if (DrawNewFrame()) {
            if (!_user_pause) {
                TimeEvents(y);

                Motion(&x, &y, &z, &a);

                cycle_count++;

                if (!(cycle_count & 0x01) && y < _water_level)
                    continue;
            }
            else
                SetMessage("", "", "PAUSED...", "", 1);

            Objects();

            Render(x, y, z, a);
        }
    }

    SwapScreens(0);

    while (_qdgdfv_key_escape)
        qdgdfv_input_poll();

    /* si se ha resuelto el ?ltimo nivel, mensaje */
    if (_last_level_resolved)
        GameEnd();
}


void GameStartup(void)
/* inicia todo lo necesario en una sesi?n de juego */
{
    RayStartup();
    MapStartup();
    MazeStartup();

    _level_resolved = 0;
    _player_dead = 0;
    _oxygen = 1000;
    _fade_edges = 1;
}


void GameShutdown(void)
/* cierra todo en una sesi?n de juego */
{
    _fade_edges = 0;
    qdgdfv_logger(_game_name,
              qdgdfv_sprintf("Shutdown - Frames per sec.: %d / %d",
                     _avg_frames_per_sec, _max_frames_per_sec));
}


void Game(void)
{
    int x, y, z, a;
    char msg0[80];
    char msg1[80];
    char msg2[80];
    char msg3[80];

    qdgdfa_stop_sound(music);

    GameStartup();

    x = 16384 + 64;
    z = 16384 + 64;
    y = 16384;
    a = ANGLE(0);

    if (_level == 0)
        strcpy(msg0, "LEVEL: CUSTOM");
    else
        sprintf(msg0, "LEVEL: %d", _level);

    sprintf(msg1, "AREA: %s", _area_name);
    sprintf(msg2, "PUMPS: %d / CONSOLES: %d", _num_pumps, _num_consoles);
    sprintf(msg3, "TOTAL SINKING ESTIMATED: %02d:%02d", _left_minutes, _left_seconds);

    SetMessage(msg0, msg1, msg2, msg3, 10);

    GameCore(x, y, z, a);

    GameShutdown();

    qdgdfa_reset();

    music = qdgdfa_load_sound("sound/holst.wav");
    qdgdfa_play_sound(music, 1);
}


void NewGame(void)
{
    do {
        ParseParam(ReadLevel());
        Game();
    } while (_level_resolved);
}


void GoLevel(void)
/* pide un nivel y su c?digo de acceso y lo lanza si es v?lido */
{
    int n_password;

    _level = GetNumber("GO LEVEL\n\n\n\n\nLEVEL NUMBER: %s\n", 30);

    if (_level == 0) {
        _level = 1;
        return;
    }

    n_password = GetNumber("GO LEVEL\n\n\n\n\nACCESS CODE: %s\n", 999998);

    ParseParam(ReadLevel());

    if (n_password == 822994)
        GameEnd();
    else if (n_password != 170868 && _password != n_password) {
        _level = 1;

        strcpy(_text_mask, "GO LEVEL\n\n\n\n\nWRONG ACCESS CODE\n");

        for (;;) {
            DrawWaterVirtualScreen();
            Draw_Text();
            qdgdfv_dump_virtual_screen();

            qdgdfv_input_poll();

            if (_qdgdfv_key_escape || _qdgdfv_key_enter)
                break;
        }

        while (_qdgdfv_key_escape || _qdgdfv_key_enter)
            qdgdfv_input_poll();
    }
    else {
        do {
            Game();
            ParseParam(ReadLevel());
        } while (_level_resolved);
    }
}


void Custom(void)
/* laberinto personalizado */
{
    _num_pumps = GetNumber("CUSTOM MAZE\n\n"
                   "NUMBER OF PUMPS THAT\n"
                   "WILL BE GENERATED\n"
                   "\n" "\n\nNUMBER OF PUMPS: [1..5]: %s\n", 5);

    if (_num_pumps == 0)
        _num_pumps = 1;

    _num_consoles = GetNumber("CUSTOM MAZE\n\n"
                  "NUMBER OF CONSOLES THAT\n"
                  "WILL BE GENERATED\n"
                  "\n" "\n\nNUMBER OF CONSOLES: [0..5]: %s\n", 5);

    _ambient_light = GetNumber("CUSTOM MAZE\n\n"
                   "THE AMBIENT LIGHT DEFINES HOW\n"
                   "DARK (4) OR BRIGHT (9)\n"
                   "WILL BE THE MAZE.\n" "\n\nAMBIENT LIGHT [4..9]: %s\n", 9);

    if (_ambient_light < 4)
        _ambient_light = 8;

    _maze_steps = GetNumber("CUSTOM MAZE\n\n"
                "THE MAZE STEPS DEFINES\n"
                "THE TOTAL SIZE OF THE MAZE.\n"
                "THE GREATER THE NUMBER OF\n"
                "PUMPS & CONSOLES, THE GREATER\n"
                "THIS NUMBER MUST BE.\n" "\n\nMAZE STEPS [15..50]: %s\n", 50);

    if (_maze_steps < 15)
        _maze_steps = 15;

    _maze_steps += _num_pumps * 5;

    if (_num_consoles)
        _maze_steps += _num_consoles * 5;

    if (_maze_steps > 100)
        _maze_steps = 100;

    _area_num = GetNumber("CUSTOM MAZE\n\n"
                  "THE AREA NUMBER DEFINES\n"
                  "THE GRAPHICS USED IN MAKING\n"
                  "THE MAZE. 1 IS 'CORRIDORS',\n"
                  "2 IS 'URANIUM SILO', AND SO ON."
                  "\n\nAREA NUMBER [1..6]: %s\n", 6);

    if (_area_num < 1)
        _area_num = 1;

    _broken_light = GetNumber("CUSTOM MAZE\n\n"
                  "THE BROKEN LIGHT MAKES\n"
                  "THE AMBIENT LIGHT BLINK.\n"
                  "\n" "\n\nBROKEN LIGHT [0..1]: %s\n", 1);

    _level = 0;

    Game();
    _level = 1;
}


void Help(void)
{
    LoadScreen("help");

    for (;;) {
        qdgdfv_dump_virtual_screen();
        qdgdfv_input_poll();

        if (_qdgdfv_key_escape || _qdgdfv_key_enter)
            break;
    }

    while (_qdgdfv_key_escape || _qdgdfv_key_enter)
        qdgdfv_input_poll();
}


void MainLoop(void)
{
    int menu_opt;

    for (;;) {
        menu_opt = Menu();

        if (menu_opt == 0)
            NewGame();
        else if (menu_opt == 1)
            GoLevel();
        else if (menu_opt == 2)
            Custom();
        else if (menu_opt == 3)
            Help();
        else if (menu_opt == -1 || menu_opt == 4)
            break;
    }
}


extern const char _binary_splumber_tar_start;
extern const char _binary_splumber_tar_end;
extern const char binary_splumber_tar_start;
extern const char binary_splumber_tar_end;


int main(int argc, char *argv[])
{
#if CONFOPT_EMBED_NOUNDER == 1
    _qdgdf_embedded_tar_start = &binary_splumber_tar_start;
    _qdgdf_embedded_tar_end = &binary_splumber_tar_end;
#else
    _qdgdf_embedded_tar_start = &_binary_splumber_tar_start;
    _qdgdf_embedded_tar_end = &_binary_splumber_tar_end;
#endif

    if (argc >= 2 && *argv[1] == '3')
        _qdgdfv_scale = 3;

    GrxStartup();

    Intro();

    MainLoop();

    GrxShutdown();

    return (0);

}
