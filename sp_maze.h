/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#define ET_NONE     0
#define ET_STATIC   1
#define ET_LIT      2
#define ET_BLINK    OT_BLINK
#define ET_BLINK_LIGHT  OT_BLINK_LIGHT
#define ET_ANIMATION    OT_ANIMATION
#define ET_SEQUENCE OT_SEQUENCE

#define MAX_STYLES 15

struct style
{
    sp_texture * wall;
    sp_texture * step;
    sp_texture * floor;
    sp_texture * ceiling;

    sp_texture * console1;
    sp_texture * console2;
    sp_texture * consoleoff;

    sp_texture * pump1off;
    sp_texture * pump1on;
    sp_texture * pump2off;
    sp_texture * pump2on;

    int n_effects;
    struct
    {
        int type;
        sp_texture * t[4];
    } effects[16];
};


extern int n_styles;
extern struct style styles[MAX_STYLES];
extern int _maze_steps;
extern int _dump_maze;
extern int _level;
extern int _password;
extern int _area_num;

extern char _area_name[];


/* prototipos */

void LoadArea(char * areafile);
char * ReadLevel(void);
void MazeStartup(void);

int BuildFloor(int wx, int wz, int style, int floor_y, int ceiling_y);
int BuildWall(int wx, int wz, int style, int dir, int floor_y, int steps);
