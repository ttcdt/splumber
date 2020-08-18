/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include <stdio.h>
#include <stdlib.h>


/* estructura de la paleta de color */
struct palette {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct palette pal[256];


int r_perc = 59;
int g_perc = 59;
int b_perc = 59;


int LoadPalette(char *palfile, struct palette *pal)
/* carga la paleta */
{
    int n;
    FILE *f;
    char lin[80];
    unsigned int r, g, b;

    if ((f = fopen(palfile, "r")) != NULL) {
        /* lee la primera línea (JASC-PAL), la segunda
           (versión) y la tercera (256) */
        fgets(lin, sizeof(lin) - 1, f);
        fgets(lin, sizeof(lin) - 1, f);
        fgets(lin, sizeof(lin) - 1, f);

        for (n = 0; n < 256; n++) {
            if (fscanf(f, "%u %u %u\n", &r, &g, &b) != 3)
                break;

            pal->r = (unsigned char) r;
            pal->g = (unsigned char) g;
            pal->b = (unsigned char) b;

            pal++;
        }

        for (; n < 256; n++) {
            pal->r = pal->g = pal->b = 0;
            pal++;
        }

        fclose(f);

        return (1);
    }

    return (0);
}


int SavePalette(char *palfile, struct palette *pal)
/* guarda la paleta */
{
    FILE *f;
    char lin[80];
    int n;

    if ((f = fopen(palfile, "w")) != NULL) {
        fprintf(f, "JASC-PAL\n");
        fprintf(f, "0100\n");
        fprintf(f, "256\n");

        for (n = 0; n < 256; n++, pal++)
            fprintf(f, "%u %u %u\n", pal->r, pal->g, pal->b);

        fclose(f);
    }

    return (0);
}


int colcmp2(const void *v1, const void *v2)
{
    int dif;

    struct palette *c1, *c2;

    c1 = (struct palette *) v1;
    c2 = (struct palette *) v2;

    dif = c1->r - c2->r;

    if (abs(c1->g - c2->g) > abs(dif))
        dif = c1->g - c2->g;

    if (abs(c1->b - c2->b) > abs(dif))
        dif = c1->b - c2->b;

    return (dif);
}


int colcmp(const void *v1, const void *v2)
{
    int d1, d2;

    struct palette *c1, *c2;

    c1 = (struct palette *) v1;
    c2 = (struct palette *) v2;

    d1 = (c1->r + c1->g + c1->b) / 3;
    d2 = (c2->r + c2->g + c2->b) / 3;

    return (d1 - d2);
}


int colcmp3(const void *v1, const void *v2)
{
    int d1, d2;

    struct palette *c1, *c2;

    c1 = (struct palette *) v1;
    c2 = (struct palette *) v2;

    d1 = c1->g << 16 | c1->r << 8 | c1->b;
    d2 = c2->g << 16 | c2->r << 8 | c2->b;

    return (d1 - d2);
}


unsigned char Degrade(unsigned char col1, unsigned char col2, int perc)
{
    int i;

    i = (int) col1 + (int) col2;
    i /= 2;

    i *= perc;
    i /= 100;

/*    i*=6;
    i/=8;
*/
    return ((unsigned char) i);
}


int PrepPalette(struct palette *pal)
{
    int n, m;

    qsort(pal, 256, sizeof(struct palette), colcmp3);

    for (n = 255, m = 127; n > 127; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
    for (n = 127, m = 63; n > 63; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
    for (n = 63, m = 31; n > 31; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
    for (n = 31, m = 15; n > 15; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
    for (n = 15, m = 7; n > 7; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
    for (n = 7, m = 3; n > 3; n -= 2, m--) {
        pal[m].r = Degrade(pal[n].r, pal[n - 1].r, r_perc);
        pal[m].g = Degrade(pal[n].g, pal[n - 1].g, g_perc);
        pal[m].b = Degrade(pal[n].b, pal[n - 1].b, b_perc);
    }
}


int main(int argc, char *argv[])
{
    int n, r, g, b;

    if (argc != 3) {
        printf("\nSpace Plumber Palette Generator - Angel Ortega\n\n");
        printf("Usage: xpal {org.pal} {des.pal}\n");
        return (1);
    }

    if (!LoadPalette(argv[1], pal)) {
        perror("LoadPalette");
        return (1);
    }

    PrepPalette(pal);

    SavePalette(argv[2], pal);

    return (0);
}
