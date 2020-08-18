/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/


/* externas */

extern int _ambient_light;
extern int _low_detail;
extern int _motion_blur;
extern int _use_motion_blur;
extern int _vertical_bias;
extern int _wave_water;
extern int _perspective_correct;

#define VERTICAL_BIAS 100

/* ángulo de visión (inicialmente, sí se puede cambiar) */
#define VISION_ANGLE    60

/* ángulo en punto fijo */
#define ANGLE(x)      (((x) * SCREEN_X_SIZE) / VISION_ANGLE)

/* una coordenada empaqueta número de bloque y offset en el bloque */
#define BLOCK_OFFSET_MASK (BLOCK_SIZE - 1)
#define BLOCK_BASE_MASK   (~ BLOCK_OFFSET_MASK)


struct block_chain
{
    int dist;   /* distancia */
    int dim;    /* factor de atenuación */
    int wx;     /* x donde choca el rayo */
    int wz;     /* y donde choca el rayo */
    int hor;    /* si el choque es horizontal o vertical */

    /* punteros a las texturas */
    sp_texture * wall;
    sp_texture * ceiling;
    sp_texture * floor;
    sp_texture * step;

    int ceiling_y;  /* y del techo */
    int wall_size;  /* tamaño del muro ó dintel */
    int floor_y;    /* y del suelo */
    int step_size;  /* tamaño del escalón */
    int idim;   /* factor de atenuación intrínseco */

    int goon;
    int empty;

    struct block_chain * next;
};


/* prototipos */

void SinCos(int a, int * sin, int * cos);
void Render(int x, int y, int z, int a);

void RayStartup(void);
void RayShutdown(void);
