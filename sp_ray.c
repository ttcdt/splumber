/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#include "sp_supp.h"
#include "sp_types.h"
#include "sp_grx.h"
#include "sp_ray.h"
#include "sp_map.h"
#include "sp_play.h"

/*******************
    Datos
********************/

/* distancia focal (no se puede cambiar, pues est? impl?cita en
   muchas operaciones optimizadas) */
#define FOCAL_DIST_BITS 7
#define FOCAL_DIST  (1<<FOCAL_DIST_BITS)

/* factor de conversi?n de grados a radianes */
#define DEG_TO_RAD (PI/180.0)


/** cadena de bloques cruzados por el rayo **/
/* esta cadena se rellena para cada rayo visual horizontal */
/* deber? apuntar adem?s al bloque real de mapa y a la textura */

/* cadena de bloques, con un l?mite arbitrario */
#define BLOCK_CHAIN_SIZE 40
static struct block_chain c_block[BLOCK_CHAIN_SIZE];

/* numero de bloques rellenos */
static int bc_num;


/** tablas trigonom?tricas **/
/* se utilizar? un grado que corresponda al giro del ?ngulo de visi?n
   para un p?xel en una pantalla de SCREEN_X_SIZE p?xels */

#define TRIG_TBL_SIZE ANGLE(360)

int sin_table[TRIG_TBL_SIZE];
int cos_table[TRIG_TBL_SIZE];
int invsin_table[TRIG_TBL_SIZE];
int invcos_table[TRIG_TBL_SIZE];

#define OPT_TVPOS

int t_vpos[SCREEN_HALF_Y_SIZE + VERTICAL_BIAS];

/*

   iluminaci?n.

   Existen 8 grados de luz/oscuridad, siendo 0 el m?s luminoso y 7 el
   m?s oscuro. Valores por encima de 7 se consideran completamente oscuros.
   El color de un p?xel (en el bitmap con valores de 255 a 129) es
   desplazado a la derecha tantas veces como indique el factor de
   atenuaci?n para ese p?xel (0 veces, iluminaci?n m?xima).

   Este factor de atenuaci?n se calcula seg?n un valor inicial intr?nseco
   de cada bloque, que define si el bloque tiene iluminaci?n normal (valor
   intr?nseco de 0) reducida (>0) o fija (-1, siempre m?xima), sumado
   al cociente de la distancia entre la luz ambiental.

   La atenuaci?n seg?n la distancia se define como (Distancia>>_ambient_light),
   y se suma al valor intr?nseco de atenuaci?n para determinar el
   desplazamiento final del valor del p?xel.

*/

/* luz ambiental */
int _ambient_light = 8;

/* low detail */
int _low_detail = 0;

/* usar low detail en movimientos */
int _use_motion_blur = 0;
int _motion_blur = 0;

/* desviaci?n vertical arriba/abajo */
int _vertical_bias = 0;

#define SCREEN_UPPER ((SCREEN_HALF_Y_SIZE)+_vertical_bias)
#define SCREEN_LOWER ((-(SCREEN_HALF_Y_SIZE))+_vertical_bias)+1

/* generar o no ondulaciones en el agua */
int _wave_water = 1;

/* usar o no correcci?n de perspectiva */
int _perspective_correct = 1;

static int _render_a;
static int _p_correct;

#ifndef PI
#define PI 3.14159265358979323846
#endif

/********************
    C?digo
*********************/

static void FillTrigTables(void)
/* rellena las tablas trigonom?tricas */
{
    int n;
    double f;
    double s, c;

    for (n = 0; n < TRIG_TBL_SIZE; n++) {
        /* calcula el ?ngulo real en radianes */
        f = (double) n;
        f *= (2 * PI) / TRIG_TBL_SIZE;

        s = sin(f);
        c = cos(f);

        sin_table[n] = (int) (s * 1024);
        cos_table[n] = (int) (c * 1024);

        if (n != ANGLE(0) && n != ANGLE(180))
            invsin_table[n] = (int) ((double) (-1024.0 / s));

        if (n != ANGLE(90) && n != ANGLE(270))
            invcos_table[n] = (int) ((double) (1024.0 / c));
    }

    for (n = 1; n < SCREEN_HALF_Y_SIZE + VERTICAL_BIAS; n++)
        t_vpos[n] = (1 << 16) / n;
}


void BuildBlockChain(int x, int y, int z, int a)
/* rellena la estructura c_block con los elementos del laberinto
   que son cruzados por el rayo. */
{
    register struct block_chain *c_bl;
    int tx, tz, vd;     /* coordenadas tx y tz y
                   variable dependiente */
    int dx, dz;     /* diferencias */
    int incx, incz;     /* incrementos */
    int h;          /* hipotenusa (distancia) */
    int l;          /* factor de luminosidad */
    int invcos;     /* inversa del coseno */
    int invsin;     /* inversa del seno */
    int dz_dx, dx_dz;   /* dz/dx y dx/dz */
    int n;

    /* corrige ?ngulos negativos y fuera de rango */
    if (a >= TRIG_TBL_SIZE)
        a -= TRIG_TBL_SIZE;
    if (a < 0)
        a += TRIG_TBL_SIZE;

    if (a >= TRIG_TBL_SIZE || a < 0)
        qdgdfv_bang("BuildBlockChain", "range(a)");

    n = _render_a - a;
    if (n >= TRIG_TBL_SIZE)
        n -= TRIG_TBL_SIZE;
    if (n < 0)
        n += TRIG_TBL_SIZE;

    _p_correct = cos_table[n];

    dx = cos_table[a];
    if (a != ANGLE(90) && a != ANGLE(270))
        invcos = invcos_table[a];
    else
        invcos = 0;

    dz = -sin_table[a];
    if (a != ANGLE(0) && a != ANGLE(180))
        invsin = invsin_table[a];
    else
        invsin = 0;

    if (dx)
        dz_dx = (dz << 12) / dx;
    else
        dz_dx = 0;

    if (dz)
        dx_dz = (dx << 12) / dz;
    else
        dx_dz = 0;

    /* calcula los incrementos */
    tx = x;

    if (a >= ANGLE(90) && a < ANGLE(270))
        incx = -BLOCK_SIZE;
    else {
        incx = BLOCK_SIZE;
        tx += BLOCK_SIZE;
    }

    /* ajusta de forma que caiga siempre en un borde */
    tx &= BLOCK_BASE_MASK;

    tz = z;

    if (a >= ANGLE(0) && a < ANGLE(180))
        incz = -BLOCK_SIZE;
    else {
        incz = BLOCK_SIZE;
        tz += BLOCK_SIZE;
    }

    tz &= BLOCK_BASE_MASK;

    c_bl = c_block;

    /* bucle de bloques */
    for (bc_num = 0; bc_num < BLOCK_CHAIN_SIZE && (dx || dz);) {
        /* calcula la vertical donde se cruza con tx */
        if (dx) {
            /* la l?nea inferior es una versi?n
               optimizada de:
               vd=z + (dz*(tx-x))/dx; */

            vd = z + ((dz_dx * (tx - x)) >> 12);

            h = ((tx - x) * invcos) >> 11;

            if (_perspective_correct)
                h = (h * _p_correct) >> 10;

            l = h >> _ambient_light;

            if (l > 7)
                l = 7;

            if (h < 0) {
                qdgdfv_bang("BuildBlockChain",
                        qdgdfv_sprintf
                        ("hor==0,h==%d,tx==%d,x==%d,invcos==%d,dz_dx==%d",
                         h, tx, x, invcos, dz_dx));
            }
            else {
                c_bl->wx = tx;
                c_bl->wz = vd;
                c_bl->dist = h;
                c_bl->dim = l;
                c_bl->hor = 0;

                if (!(c_bl->goon = MapBlockChain(c_bl, y, incx, incz)))
                    dx = 0;

                c_bl++;
                bc_num++;
            }
        }

        /* lo mismo, para la horizontal */
        if (dz && bc_num < BLOCK_CHAIN_SIZE) {
            /* La l?nea inferior es una versi?n
               optimizada de
               vd=x + (dx*(tz-z))/dz; */

            vd = x + ((dx_dz * (tz - z)) >> 12);

            h = ((tz - z) * invsin) >> 11;

            if (_perspective_correct)
                h = (h * _p_correct) >> 10;

            l = h >> _ambient_light;

            if (l > 7)
                l = 7;

            if (h < 0) {
                qdgdfv_bang("BuildBlockChain",
                        qdgdfv_sprintf
                        ("hor==1,h==%d,tx==%d,x==%d,invsin==%d", h, tx, x,
                         invsin));
            }
            else {
                c_bl->wx = vd;
                c_bl->wz = tz;
                c_bl->dist = h;
                c_bl->dim = l;
                c_bl->hor = 1;

                if (!(c_bl->goon = MapBlockChain(c_bl, y, incx, incz)))
                    dz = 0;

                c_bl++;
                bc_num++;
            }
        }

        tx += incx;
        tz += incz;
    }
}


struct block_chain *SortBlockChain(void)
/* ordena la cadena de bloques */
{
    register int n;
    register struct block_chain *b;
    register struct block_chain *f;
    struct block_chain *t1;
    struct block_chain *t2;

    f = b = c_block;
    b++;

    f->next = NULL;

    for (n = bc_num - 1; n; n--) {
        if (b->dist > f->dist) {
            b->next = f;
            f = b;
        }
        else {
            t1 = f;
            t2 = f->next;

            while (t2 != NULL && t2->dist > b->dist) {
                t1 = t2;
                t2 = t2->next;
            }

            b->next = t1->next;
            t1->next = b;
        }

        b++;
    }

    return (f);
}


/*

    proyecci?n y escalado (muros)

    Seg?n la figura


    |<----------------- dist ---------------->|

            |       ------I
            |     ------      I
            |   ------        I
              ------              I vsize
        ------  I p_vsize         I
      ------    I             I
ojo ------------------------------------------I
            |
    |<----- fd -------->|
    (dist. focal)

             pantalla
             donde se
             proyecta


    El objetivo es calcular p_vsize, que nos dice cu?ntos p?xels
    verticales hay que pintar realmente, y un factor que nos permita
    conocer el siguiente punto a pintar en el bitmap.

    Por proyecci?n de tri?ngulos, y teniendo un fd (distancia focal)
    de 128 (que nos conviene, porque se puede dividir y mult. con >> y <<),
    nos da que

        p_vsize=(vsize * fd) / dist

    Para calcular el incremento del bitmap (bi), se calcula seg?n la
    l?gica: el bitmap, a la distancia focal, debe estar en relaci?n de 1
    a 1. Dado que el bitmap mide 128 p?xels de alto, nos vuelve a
    convenir usar un fd de 128, de forma que 128/128==1. De esto se
    deduce que

        bi=dist / fd

    Y si h?bilmente usamos exactamente 7 bits de aritm?tica en punto
    fijo, nos basta con hacer

        bi=dist

    de forma que el siguiente p?xel del bitmap a pintar lo calculamos,
    simplemente, sumando dist a un offset (que, obviamente, se dividir?
    entre 128 para despreciar los decimales y se har? un & 127 para
    que nunca exceda del tama?o del bitmap).

*/

#ifndef NODEBUG
#define ASSERT_SCREEN(f)    qdgdfv_assert_in_virtual_screen(f,(unsigned char *)screen);
#else
#define ASSERT_SCREEN(f)    (void) 0;
#endif

void DrawWallScanLine(char *screen, sp_texture * bmp, int vpos,
              int vsize, char dim, char idim, int dist)
/* dibuja una l?nea vertical de un bitmap */
{
    int p_vpos;     /* vpos, proyectada */
    int p_vsize;        /* vsize, proyectado */
    int offset;     /* valor que se suma a bmp */
    register int toffset;   /* offset temporal */
    register unsigned char a;   /* p?xel */
    int f;

    if (dist == 0 || bmp == NULL || vsize <= 0)
        return;

    p_vpos = (vpos << FOCAL_DIST_BITS) / dist;

    /* si est? por debajo del campo visual, fuera */
    if (p_vpos < SCREEN_LOWER)
        return;

    dim += idim;
    if (dim < 0)
        dim = 0;

    p_vsize = (vsize << FOCAL_DIST_BITS) / dist;

    if (p_vpos - p_vsize <= SCREEN_LOWER)
        p_vsize = SCREEN_HALF_Y_SIZE - _vertical_bias + p_vpos - 1;
    p_vsize++;

    offset = 0;

    if (p_vpos > SCREEN_UPPER) {
        f = p_vpos - SCREEN_UPPER;

        offset = dist * f;
        p_vsize -= f;
    }
    else
        screen += (SCREEN_HALF_Y_SIZE - p_vpos + _vertical_bias) * _qdgdfv_screen_x_size;

    while (p_vsize > 0) {
        toffset = offset >> BLOCK_BITS;
        toffset &= BLOCK_MASK;

        a = *(bmp + toffset);

        /* se calcula la atenuaci?n */
        a >>= dim;

        ASSERT_SCREEN("DrawWallScanLine");
        *screen = a;

        /* se calcula la siguiente posici?n en el bmp */
        offset += dist;

        screen += _qdgdfv_screen_x_size;

        p_vsize--;
    }
}


void DrawWallScanLineLo(char *screen, sp_texture * bmp, int vpos,
            int vsize, char dim, char idim, int dist)
/* dibuja una l?nea vertical de un bitmap (en low detail) */
{
    int p_vpos;     /* vpos, proyectada */
    int p_vsize;        /* vsize, proyectado */
    int offset;     /* valor que se suma a bmp */
    register int toffset;   /* offset temporal */
    register unsigned char a;   /* p?xel */
    int f;

    if (dist == 0 || bmp == NULL || vsize <= 0)
        return;

    p_vpos = (vpos << FOCAL_DIST_BITS) / dist;

    /* si est? por debajo del campo visual, fuera */
    if (p_vpos < SCREEN_LOWER)
        return;

    dim += idim;
    if (dim < 0)
        dim = 0;

    p_vsize = (vsize << FOCAL_DIST_BITS) / dist;

    if (p_vpos - p_vsize <= SCREEN_LOWER)
        p_vsize = SCREEN_HALF_Y_SIZE - _vertical_bias + p_vpos - 1;
    p_vsize++;

    offset = 0;

    if (p_vpos > SCREEN_UPPER) {
        f = p_vpos - SCREEN_UPPER;

        offset = dist * f;
        p_vsize -= f;
    }
    else
        screen += (SCREEN_HALF_Y_SIZE - p_vpos + _vertical_bias) * _qdgdfv_screen_x_size;

    while (p_vsize > 0) {
        toffset = offset >> BLOCK_BITS;
        toffset &= BLOCK_MASK;

        a = *(bmp + toffset);

        /* se calcula la atenuaci?n */
        a >>= dim;

        ASSERT_SCREEN("DrawWallScanLineLo");
        *screen = a;
        *(screen + 1) = a;

        /* se calcula la siguiente posici?n en el bmp */
        offset += dist;

        screen += _qdgdfv_screen_x_size;

        p_vsize--;
    }
}


/*

    proyecci?n y escalado (suelos y techos).

                 P
    ...............|.............======....    -
           |     --------          ^
          -------    \             altura
       ------- |        A |            V
ojo -------........|.......................    -
           |
           |
           |
    ...............|.......................

    |<---- fd ---->|

    ====== techo a pintar

*/


void DrawFloorScanLine(char *screen, sp_texture * bmp, int vpos,
               int wx, int wz, int va, char idim, int dist, int mdist)
/* dibuja una l?nea vertical de un suelo */
{
    int p_vpos;
    int odist;
    register unsigned char a;
    register int n;
    register int sin_128, cos_128;
    register unsigned char dim;
    int toffset;

    if (bmp == NULL)
        return;

    if (dist == 0)
        dist = 1;

    if (va >= TRIG_TBL_SIZE)
        va -= TRIG_TBL_SIZE;
    if (va < 0)
        va += TRIG_TBL_SIZE;

    wx &= BLOCK_MASK;
    wz &= BLOCK_MASK;

    vpos <<= FOCAL_DIST_BITS;

    p_vpos = vpos / dist;

    if (p_vpos >= 0)
        return;

    /* convertir en sen(va)*128 */
    sin_128 = sin_table[va] >> 2;
    cos_128 = cos_table[va] >> 2;

    wx <<= BLOCK_BITS;
    wz <<= BLOCK_BITS;

    /* si est? demasiado cerca, se calcula la primera distancia
       que se puede ver */
    if (p_vpos < SCREEN_LOWER) {
        p_vpos = SCREEN_LOWER;

        odist = dist;
        dist = vpos / p_vpos;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);
    }

    screen += (SCREEN_HALF_Y_SIZE + (-p_vpos) + _vertical_bias) * _qdgdfv_screen_x_size;

    while (p_vpos < 0) {
        if (idim == -1)
            dim = 0;
        else
            dim = (dist >> _ambient_light) + idim;

        /* calcula el componente z, s?lo quitando los decimales */
        toffset = wz & (BLOCK_MASK << BLOCK_BITS);

        /* a?ade el componente x */
        toffset |= (wx >> BLOCK_BITS) & BLOCK_MASK;

        a = *(bmp + toffset);

        a >>= dim;

        ASSERT_SCREEN("DrawFloorScanLine");
        *screen = a;

        /* calcula la distancia a la que est? el
           siguiente punto */
        p_vpos++;

        if (p_vpos == 0)
            break;

        odist = dist;
#ifdef OPT_TVPOS
        dist = vpos * (-t_vpos[-p_vpos]);
        dist >>= 16;
#else
        dist = vpos / p_vpos;
#endif

        if (dist > mdist)
            break;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);

        screen -= _qdgdfv_screen_x_size;
    }
}


void DrawCeilingScanLine(char *screen, sp_texture * bmp, int vpos,
             int wx, int wz, int va, char idim, int dist, int mdist)
/* dibuja una l?nea vertical de un techo */
{
    int p_vpos;
    int odist;
    register unsigned char a;
    register int n;
    register int sin_128, cos_128;
    register unsigned char dim;
    int toffset;

    if (bmp == NULL)
        return;

    if (dist == 0)
        dist = 1;

    if (va >= TRIG_TBL_SIZE)
        va -= TRIG_TBL_SIZE;
    if (va < 0)
        va += TRIG_TBL_SIZE;

    wx &= BLOCK_MASK;
    wz &= BLOCK_MASK;

    vpos <<= FOCAL_DIST_BITS;

    p_vpos = vpos / dist;

    if (p_vpos <= 0)
        return;

    /* convertir en sen(va)*128 */
    sin_128 = sin_table[va] >> 2;
    cos_128 = cos_table[va] >> 2;

    wx <<= BLOCK_BITS;
    wz <<= BLOCK_BITS;

    /* si est? demasiado cerca, se calcula la primera distancia
       que se puede ver */
    if (p_vpos > SCREEN_UPPER) {
        p_vpos = SCREEN_UPPER;

        odist = dist;
        dist = vpos / p_vpos;

        if (dist > mdist)
            return;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);
    }
    else
        screen += (SCREEN_HALF_Y_SIZE - p_vpos + _vertical_bias)
            * _qdgdfv_screen_x_size;

    while (p_vpos > 0) {
        if (idim == -1)
            dim = 0;
        else
            dim = (dist >> _ambient_light) + idim;

        /* calcula el componente z, s?lo quitando los decimales */
        toffset = wz & (BLOCK_MASK << BLOCK_BITS);

        /* a?ade el componente x */
        toffset |= (wx >> BLOCK_BITS) & BLOCK_MASK;

        a = *(bmp + toffset);

        a >>= dim;

        ASSERT_SCREEN("DrawCeilingScanLine");
        *screen = a;

        /* calcula la distancia a la que est? el
           siguiente punto */
        p_vpos--;

        if (p_vpos == 0)
            break;

        odist = dist;

#ifdef OPT_TVPOS
        dist = vpos * t_vpos[p_vpos];
        dist >>= 16;
#else
        dist = vpos / p_vpos;
#endif

        if (dist > mdist)
            break;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);

        screen += _qdgdfv_screen_x_size;
    }
}


void DrawFloorScanLineLo(char *screen, sp_texture * bmp, int vpos,
             int wx, int wz, int va, char idim, int dist, int mdist)
/* dibuja una l?nea vertical de un suelo */
{
    int p_vpos;
    int odist;
    register unsigned char a;
    register int n;
    register int sin_128, cos_128;
    register unsigned char dim;
    int toffset;

    if (bmp == NULL)
        return;

    if (dist == 0)
        dist = 1;

    if (va >= TRIG_TBL_SIZE)
        va -= TRIG_TBL_SIZE;
    if (va < 0)
        va += TRIG_TBL_SIZE;

    wx &= BLOCK_MASK;
    wz &= BLOCK_MASK;

    vpos <<= FOCAL_DIST_BITS;

    p_vpos = vpos / dist;

    if (p_vpos >= 0)
        return;

    /* convertir en sen(va)*128 */
    sin_128 = sin_table[va] >> 2;
    cos_128 = cos_table[va] >> 2;

    wx <<= BLOCK_BITS;
    wz <<= BLOCK_BITS;

    /* si est? demasiado cerca, se calcula la primera distancia
       que se puede ver */
    if (p_vpos < SCREEN_LOWER) {
        p_vpos = SCREEN_LOWER;

        odist = dist;
        dist = vpos / p_vpos;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);
    }

    screen += (SCREEN_HALF_Y_SIZE + (-p_vpos) + _vertical_bias) * _qdgdfv_screen_x_size;

    while (p_vpos < 0) {
        if (idim == -1)
            dim = 0;
        else
            dim = (dist >> _ambient_light) + idim;

        /* calcula el componente z, s?lo quitando los decimales */
        toffset = wz & (BLOCK_MASK << BLOCK_BITS);

        /* a?ade el componente x */
        toffset |= (wx >> BLOCK_BITS) & BLOCK_MASK;

        a = *(bmp + toffset);

        a >>= dim;

        ASSERT_SCREEN("DrawFloorScanLineLo");
        *screen = a;
        *(screen + 1) = a;

        /* calcula la distancia a la que est? el
           siguiente punto */
        p_vpos++;

        if (p_vpos == 0)
            break;

        odist = dist;

#ifdef OPT_TVPOS
        dist = vpos * (-t_vpos[-p_vpos]);
        dist >>= 16;
#else
        dist = vpos / p_vpos;
#endif

        if (dist > mdist)
            break;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);

        screen -= _qdgdfv_screen_x_size;
    }
}


void DrawCeilingScanLineLo(char *screen, sp_texture * bmp, int vpos,
               int wx, int wz, int va, char idim, int dist, int mdist)
/* dibuja una l?nea vertical de un techo (en low) */
{
    int p_vpos;
    int odist;
    register unsigned char a;
    register int n;
    register int sin_128, cos_128;
    register unsigned char dim;
    int toffset;

    if (bmp == NULL)
        return;

    if (dist == 0)
        dist = 1;

    if (va >= TRIG_TBL_SIZE)
        va -= TRIG_TBL_SIZE;
    if (va < 0)
        va += TRIG_TBL_SIZE;

    wx &= BLOCK_MASK;
    wz &= BLOCK_MASK;

    vpos <<= FOCAL_DIST_BITS;

    p_vpos = vpos / dist;

    if (p_vpos <= 0)
        return;

    /* convertir en sen(va)*128 */
    sin_128 = sin_table[va] >> 2;
    cos_128 = cos_table[va] >> 2;

    wx <<= BLOCK_BITS;
    wz <<= BLOCK_BITS;

    /* si est? demasiado cerca, se calcula la primera distancia
       que se puede ver */
    if (p_vpos > SCREEN_UPPER) {
        p_vpos = SCREEN_UPPER;

        odist = dist;
        dist = vpos / p_vpos;

        if (dist > mdist)
            return;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);
    }
    else
        screen += (SCREEN_HALF_Y_SIZE - p_vpos + _vertical_bias) * _qdgdfv_screen_x_size;

    while (p_vpos > 0) {
        if (idim == -1)
            dim = 0;
        else
            dim = (dist >> _ambient_light) + idim;

        /* calcula el componente z, s?lo quitando los decimales */
        toffset = wz & (BLOCK_MASK << BLOCK_BITS);

        /* a?ade el componente x */
        toffset |= (wx >> BLOCK_BITS) & BLOCK_MASK;

        a = *(bmp + toffset);

        a >>= dim;

        ASSERT_SCREEN("DrawCeilingScanLineLo");
        *screen = a;
        *(screen + 1) = a;

        /* calcula la distancia a la que est? el
           siguiente punto */
        p_vpos--;

        if (p_vpos == 0)
            break;

        odist = dist;

#ifdef OPT_TVPOS
        dist = vpos * t_vpos[p_vpos];
        dist >>= 16;
#else
        dist = vpos / p_vpos;
#endif

        if (dist > mdist)
            break;

        /* localiza el siguiente punto del bitmap */
        n = dist - odist;

        wx += (cos_128 * n);
        wz -= (sin_128 * n);

        screen += _qdgdfv_screen_x_size;
    }
}


void RenderHigh(int x, int y, int z, int a)
/* dibuja un frame completo sin low detail */
{
    struct block_chain *b;
    char *screen;
    int v;
    int odist;

    screen = (char *) _qdgdfv_virtual_screen + screen_offset;

    for (v = a + SCREEN_X_SIZE / 2; v > a - SCREEN_X_SIZE / 2; v--) {
        BuildBlockChain(x, y, z, v);

        b = SortBlockChain();

        odist = -1;

        while (b != NULL) {
            if (b->empty) {
                b = b->next;
                continue;
            }

            /* pinta el techo */
            DrawCeilingScanLine(screen,
                        b->ceiling, b->ceiling_y,
                        b->wx, b->wz, v, b->idim, b->dist, odist);

            /* pinta el suelo */
            DrawFloorScanLine(screen,
                      b->floor, b->floor_y,
                      b->wx, b->wz, v, b->idim, b->dist, odist);

            /* pinta el muro ? dintel */
            DrawWallScanLine(screen,
                     b->wall, b->ceiling_y + b->wall_size,
                     b->wall_size, b->dim, b->idim, b->dist);

            /* pinta el escal?n */
            DrawWallScanLine(screen,
                     b->step, b->floor_y,
                     b->step_size, b->dim, b->idim, b->dist);

            odist = b->dist;

            b = b->next;
        }

        /* dibuja el techo y el suelo en el que
           est? el jugador */
        b = c_block;

        if (MapCurPos(b, x, y, z)) {
            DrawCeilingScanLine(screen,
                        b->ceiling, b->ceiling_y, x, z, v, 0, 1, odist);

            DrawFloorScanLine(screen, b->floor, b->floor_y, x, z, v, 0, 1, odist);
        }

        screen++;
    }
}


void RenderLow(int x, int y, int z, int a)
/* dibuja un frame completo en low detail */
{
    struct block_chain *b;
    char *screen;
    int v;
    int odist;

    screen = (char *) _qdgdfv_virtual_screen + screen_offset;

    for (v = a + SCREEN_X_SIZE / 2; v > a - SCREEN_X_SIZE / 2; v -= 2) {
        BuildBlockChain(x, y, z, v);

        b = SortBlockChain();

        odist = -1;

        while (b != NULL) {
            if (b->empty) {
                b = b->next;
                continue;
            }

            /* pinta el techo */
            DrawCeilingScanLineLo(screen,
                          b->ceiling, b->ceiling_y,
                          b->wx, b->wz, v, b->idim, b->dist, odist);

            /* pinta el suelo */
            DrawFloorScanLineLo(screen,
                        b->floor, b->floor_y,
                        b->wx, b->wz, v, b->idim, b->dist, odist);

            /* pinta el muro ? dintel */
            DrawWallScanLineLo(screen,
                       b->wall, b->ceiling_y + b->wall_size,
                       b->wall_size, b->dim, b->idim, b->dist);

            /* pinta el escal?n */
            DrawWallScanLineLo(screen,
                       b->step, b->floor_y,
                       b->step_size, b->dim, b->idim, b->dist);

            odist = b->dist;

            b = b->next;
        }

        /* dibuja el techo y el suelo en el que
           est? el jugador */
        b = c_block;

        if (MapCurPos(b, x, y, z)) {
            DrawCeilingScanLineLo(screen,
                          b->ceiling, b->ceiling_y, x, z, v, 0, 1, odist);

            DrawFloorScanLineLo(screen,
                        b->floor, b->floor_y, x, z, v, 0, 1, odist);
        }

        screen += 2;
    }
}


void Render(int x, int y, int z, int a)
/* dibuja un frame completo */
{
    _render_a = a;

    if (y <= _water_level) {
        SwapScreens(1);
        SetPalette(1);

        RenderLow(x, y, z, a);
        SwapScreens(0);

        if (_wave_water)
            DrawWaterVirtualScreen();
    }
    else {
        SetPalette(0);

        if (_low_detail || _motion_blur)
            RenderLow(x, y, z, a);
        else
            RenderHigh(x, y, z, a);
    }

    PlayerDrawMask();
    qdgdfv_dump_virtual_screen();

    if (_wave_water)
        WaveWaterTexture();

    _motion_blur = 0;
}


void SinCos(int a, int *sin, int *cos)
/* devuelve el seno y el coseno de a */
{
    *sin = sin_table[a];
    *cos = cos_table[a];
}


void RayStartup(void)
/* llama a las rutinas necesarias para arrancar */
{
    FillTrigTables();
}


void RayShutdown(void)
/* cierra el render */
{
}
