/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* tamaño en elementos del mapa */

#define MAP_X_SIZE_BITS 8
#define MAP_X_SIZE  (1 << MAP_X_SIZE_BITS)
#define MAP_Z_SIZE  MAP_X_SIZE
#define MAP_SIZE    ((MAP_X_SIZE) * (MAP_Z_SIZE))

/*#define MAP_OFFSET(wx,wz) (MAP_X_SIZE-(wx))+((wz)<<MAP_X_SIZE_BITS)*/
#define MAP_OFFSET(wx,wz) ((wx)) + ((wz) << MAP_X_SIZE_BITS)


/* objetos */

#define OT_NONE     0
#define OT_PUMP     1
#define OT_CONSOLE  2
#define OT_BLINK    100
#define OT_BLINK_LIGHT  101
#define OT_ANIMATION    102
#define OT_SEQUENCE 103

typedef struct _sp_object
{
    int type;         /* tipo de objeto */
    int status;       /* estado */
    sp_texture * t[4];    /* texturas */
    int idim;         /* atenuación */
    int clicks;       /* contador de ciclos */

    struct _sp_object * next; /* puntero al siguiente */
    struct map_face * face;   /* puntero a la cara que lo contiene */
} sp_object;


/* cada una de las 4 caras de un elemento del mapa */

struct map_face
{
    int wall_size;          /* a sumar a map_block.ceiling_y */
    int step_size;          /* a restar a map_block.floor_y */

    sp_object * object;     /* objeto */
    int idim;           /* factor de atenuación */
    sp_texture * wall;      /* textura del muro ó dintel */
    sp_texture * step;      /* textura del zócalo */
};


/* cada elemento del mapa */

struct map_block
{
    struct map_face * faces[4]; /* las 4 caras */

    int ceiling_y;          /* altura del techo */
    int floor_y;            /* altura del suelo */

    sp_texture * ceiling;       /* textura del techo */
    sp_texture * floor;     /* textura del suelo */
};


/* externas */

extern int _water_level;
extern struct map_block ** m_block;
extern sp_object * objects;
extern char _area_file[];


/* prototipos */

void MapStartup(void);

struct map_block * AllocMapBlock(void);
struct map_face * AllocMapFace(void);
sp_object * AllocObject(struct map_face * face);

int MapBlockChain(struct block_chain * b, int y, int incx, int incz);
int MapCurPos(struct block_chain * b, int x, int y, int z);

void OptimizeMap(void);
