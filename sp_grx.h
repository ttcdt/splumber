/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* externas */

extern sp_texture * _original_water_texture;
extern sp_texture * _water_texture;

extern char * _water_virtual_screen;

extern void (* DrawMask)(char *);

extern char _font_file[150];

extern int _fade_edges;

/* prototipos */

void LoadPalette(char * palfile);
void LoadScreen(char * part_screen);
void SwapScreens(int water);
void SetPalette(int water);
void SetupPalettes(void);

sp_texture * LoadTexture (char * pcxfile);
void DrawWaterVirtualScreen(void);

void WaveWaterTexture(void);

void GrxStartup(void);
void GrxShutdown(void);
