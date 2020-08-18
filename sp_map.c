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

#include "sp_supp.h"
#include "sp_types.h"
#include "sp_ray.h"
#include "sp_grx.h"
#include "sp_map.h"


/* puntero al array de punteros a elementos del mapa */
struct map_block **m_block = NULL;

/* nivel general del agua */
int _water_level;

/* cadena de objetos */
sp_object *objects = NULL;


/********************
    Código
*********************/


struct map_block *AllocMapBlock(void)
{
    struct map_block *m;

    m = qdgdfv_malloc(sizeof(struct map_block));
    memset(m, '\0', sizeof(struct map_block));

    return (m);
}


struct map_face *AllocMapFace(void)
{
    struct map_face *f;

    f = qdgdfv_malloc(sizeof(struct map_face));
    memset(f, '\0', sizeof(struct map_face));

    return (f);
}


sp_object *AllocObject(struct map_face * face)
{
    sp_object *o;

    o = qdgdfv_malloc(sizeof(sp_object));
    memset(o, '\0', sizeof(sp_object));

    /* asocia ambos */
    o->face = face;
    face->object = o;

    /* coge el dim */
    o->idim = face->idim;

    /* actualiza la cadena */
    o->next = objects;
    objects = o;

    return (o);
}


/*

    Los dos tipos de bloque

    MURO OPACO:

          +--------+     -
          |        |     ^
          |        |     |
          |        |     | wall_size            step_size==0
          |        |     |
          |        |     |
          |        |     V
          |        |     -
    ======+--------+======    -
                              ^
                              |
                              | ceiling_y (==floor_y)
                              .

    BLOQUE ACCESIBLE:

          +--------+  -
          |        |  ^
          |        |  | wall_size
          |        |  V
          +--------+  -             -
                                    ^
                                    |
                                    | ceiling_y
          +--------+  -             |             -
          |        |  ^             |             ^
          |        |  | step_size   |             | floor_y
          |        |  V             |             |
    ======+--------+= -             .             .

*/


int MapBlockChain(struct block_chain *b, int y, int incx, int incz)
/* rellena la parte de block_chain referida al mapa */
{
    struct map_block *m;
    struct map_face *f;
    int face;
    int wx;
    int wz;
    int vd;
    int goon;
    int t;

    wx = b->wx >> BLOCK_BITS;
    wz = b->wz >> BLOCK_BITS;

    /* si se salen del mapa, dejar de trazar rayos por aquí */
    if (wx < 0 || wx >= MAP_X_SIZE || wz < 0 || wz >= MAP_Z_SIZE)
        return (0);

    /* calcula qué cara se ve desde ese punto de vista */
    if (b->hor) {
        if (incz < 0) {
            face = 3;
            wz--;
        }
        else {
            face = 1;
        }

        vd = (b->wx & BLOCK_MASK);
        if (face == 3)
            vd = 127 - vd;

        vd <<= BLOCK_BITS;
    }
    else {
        if (incx < 0) {
            face = 0;
            wx--;
        }
        else {
            face = 2;
        }

        vd = (b->wz & BLOCK_MASK);
        if (face == 2)
            vd = 127 - vd;

        vd <<= BLOCK_BITS;
    }

    /* localiza el bloque de mapa al que corresponden
       las coordenadas */
    m = *(m_block + MAP_OFFSET(wx, wz));

/*    qdgdfv_logger("MapBlockChain",qdgdfv_sprintf("face==%d, b->hor==%d,b->dist==%d,b->wx==%d,b->wz==%d,wx==%d,wz==%d,MAP_OFFSET==%d",face,b->hor,b->dist,b->wx,b->wz,wx,wz,MAP_OFFSET(wx,wz)));
*/
    /* si aquí no hay nada, volver y no seguir buscando en este rayo */
    if (m == NULL) {
        b->empty = 1;
        return (0);
    }

    /* coge la cara visible */
    f = m->faces[face];

    if (f == NULL) {
        b->empty = 1;
        return (0);
    }

    b->empty = 0;

    /* si el techo y el suelo es el mismo, es un muro y no
       hay que seguir buscando */
    goon = !(m->ceiling_y == m->floor_y);

/*    qdgdfv_logger("MapBlockChain",qdgdfv_sprintf("f==%X,[0]=%X,[1]=%X,[2]=%X,[3]=%X",f,m->faces[0],m->faces[1],m->faces[2],m->faces[3]));
*/
    /* coge las texturas y la atenuación */
    b->ceiling = m->ceiling;
    b->floor = m->floor;
    b->idim = f->idim;

    if (f->wall)
        b->wall = f->wall + vd;

    if (f->step)
        b->step = f->step + vd;

    if (y > _water_level) {
        /* sobre el agua */

        if (m->floor_y < _water_level) {
            /* el suelo está por debajo del agua */
            if (goon) {
                /* si no es un muro, lo que se
                   ve de suelo es la superficie del
                   agua */

                b->floor = _water_texture;
                b->floor_y = _water_level - y;
            }
            else
                b->floor_y = 0;

            b->step = NULL;
            b->step_size = -1;

            /* calcular si el techo también está
               debajo del agua */
            if (m->ceiling_y < _water_level) {
                /* si es así, calcular cuánto muro se ve */
                b->ceiling_y = _water_level - y;
                b->wall_size = f->wall_size - (_water_level - m->ceiling_y);

                /* y además, no se verá ni la superficie
                   ni nada de lo que hubiera más allá */
                b->floor = NULL;
                goon = 0;
            }
            else {
                /* se verá de forma normal */
                b->ceiling_y = m->ceiling_y - y;
                b->wall_size = f->wall_size;
            }
        }
        else {
            b->floor_y = m->floor_y - y;

            /* el suelo está por encima del agua: calcular
               si el zócalo se hunde la superficie */
            if (m->floor_y - f->step_size < _water_level)
                b->step_size = m->floor_y - _water_level;
            else
                b->step_size = f->step_size;

            /* si el suelo está por encima del agua,
               el techo, aún más; guardar tal cual */
            b->ceiling_y = m->ceiling_y - y;
            b->wall_size = f->wall_size;
        }
    }
    else {
        /* bajo el agua */

        if (m->ceiling_y > _water_level) {
            /* el techo está por encima del agua: no
               se ve la parte superior */
            if (goon) {
                /* si no es un muro, lo que se
                   ve de techo es la superficie
                   del agua */

                b->ceiling = _water_texture;
                b->ceiling_y = _water_level - y;
            }
            else
                b->ceiling_y = 0;

            b->wall = NULL;
            b->wall_size = -1;

            /* calcular si el suelo también está
               por encima del agua */

            if (m->floor_y > _water_level) {
                /* si es así, calcular cuánto muro se ve */
                b->floor_y = _water_level - y;
                b->step_size = f->step_size - (m->floor_y - _water_level);

                /* además, saltar en la textura del
                   escalón lo que no se ve por estar
                   fuera del agua */
                if (b->step)
                    b->step += (m->floor_y - _water_level);

                /* y además, no se verá ni la superficie
                   ni nada de lo que hubiera más allá */
                b->ceiling = NULL;
                goon = 0;
            }
            else {
                /* se verá de forma normal */
                b->floor_y = m->floor_y - y;
                b->step_size = f->step_size;
            }
        }
        else {
            b->ceiling_y = m->ceiling_y - y;

            /* el techo está por debajo del agua: calcular
               si el muro sobrepasa la superficie */
            if (m->ceiling_y + f->wall_size > _water_level) {
                /* le suma a la textura tantos
                   píxels como se pierden por encima */
                t = m->ceiling_y + f->wall_size - _water_level;
                b->wall += t;

                b->wall_size = _water_level - m->ceiling_y;
            }
            else
                b->wall_size = f->wall_size;

            /* si el techo está por debajo del agua,
               el suelo, aún más; guardar tal cual */
            b->floor_y = m->floor_y - y;
            b->step_size = f->step_size;
        }
    }

    return (goon);
}


int MapCurPos(struct block_chain *b, int x, int y, int z)
/* Dibuja el techo y el suelo en el que está el jugador. Es una
   versión simplificada de MapBlockChain */
{
    struct map_block *m;

    x >>= BLOCK_BITS;
    z >>= BLOCK_BITS;

    m = *(m_block + MAP_OFFSET(x, z));

    if (m != NULL) {
        if (y > _water_level) {
            if (m->floor_y < _water_level) {
                b->floor = _water_texture;
                b->floor_y = _water_level - y;
            }
            else {
                b->floor = m->floor;
                b->floor_y = m->floor_y - y;
            }

            b->ceiling = m->ceiling;
            b->ceiling_y = m->ceiling_y - y;
        }
        else {
            if (m->ceiling_y > _water_level) {
                b->ceiling = _water_texture;
                b->ceiling_y = _water_level - y;
            }
            else {
                b->ceiling = m->ceiling;
                b->ceiling_y = m->ceiling_y - y;
            }

            b->floor = m->floor;
            b->floor_y = m->floor_y - y;
        }

        return (1);
    }

    return (0);
}


static void OptimizeFace(struct map_block *m, int x, int z, int face)
/* Optimiza un bloque del mapa */
{
    struct map_block *a;
    struct map_face *f;

    if (m == NULL)
        return;

    f = m->faces[face];

    if (f == NULL)
        return;

    a = *(m_block + MAP_OFFSET(x, z));

    if (a == NULL)
        return;

    f->wall_size = (a->ceiling_y - m->ceiling_y);
    f->step_size = (m->floor_y - a->floor_y);
}


void OptimizeMap(void)
/* optimiza el mapa, eliminando trozos de muros que no se ven */
{
    int x, z;
    struct map_block *m;

    for (z = 1; z < MAP_Z_SIZE - 1; z++) {
        for (x = 1; x < MAP_X_SIZE - 1; x++) {
            m = *(m_block + MAP_OFFSET(x, z));

            OptimizeFace(m, x + 1, z, 0);
            OptimizeFace(m, x, z - 1, 1);
            OptimizeFace(m, x - 1, z, 2);
            OptimizeFace(m, x, z + 1, 3);
        }
    }
}


void MapStartup(void)
/* inicializa el laberinto */
{
    size_t map_size;

    map_size = MAP_SIZE * sizeof(struct map_block *);

    if (m_block == NULL)
        m_block = (struct map_block **) qdgdfv_malloc(map_size);
    else {
        int n, i;
        struct map_block *m;
        struct map_face *f;

        for (n = 0; n < MAP_SIZE; n++) {
            if ((m = m_block[n]) == NULL)
                continue;

            for (i = 0; i < 4; i++) {
                if ((f = m->faces[i]) != NULL) {
                    if (f->object != NULL)
                        free(f->object);

                    free(f);
                }
            }
        }

        objects = NULL;
    }

    memset(m_block, '\0', map_size);
}
