/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

/* variables */

extern int _frames_per_sec;
extern int _avg_frames_per_sec;
extern int _max_frames_per_sec;
extern int _water_speed;
extern int _water_limit;
extern int _left_minutes;
extern int _left_seconds;
extern int _oxygen;
extern int _player_dead;
extern int _level_resolved;
extern int _last_level_resolved;
extern int _show_frames_per_sec;
extern int _num_pumps;
extern int _num_consoles;
extern int _random_seed;
extern int _never_die;
extern int _no_timing;
extern int _broken_light;
extern int _float;
extern int _run;
extern int _user_pause;

extern char * _game_name;


/* tamaños del jugador */

#define PS_TOTAL    64        /* tamaño del cuerpo entero */
#define PS_HEAD     8         /* tamaño de la cabeza */
#define PS_LEGS     28        /* tamaño de las piernas */
#define PS_BODY     PS_TOTAL-PS_LEGS  /* tamaño del cuerpo */
#define PS_DEAD     8         /* tamaño del ahogado */


/* velocidades */

#define PV_FORWARD  18      /* andando hacia adelante */
#define PV_BACK     18      /* andando hacia atrás (10) */
#define PV_FAST_FWD 40      /* corriendo hacia adelante */
#define PV_FAST_BACK    40      /* corriendo hacia atrás (22) */
#define PV_FALL     4       /* cayendo */
#define PV_WATER_FALL   1       /* hundiéndose */
#define PV_SWIM_FWD 8       /* nadando hacia adelante */
#define PV_SWIM_BACK    8       /* nadando hacia atrás */
#define PV_WATER_FWD    6       /* buceando hacia adelante */
#define PV_WATER_BACK   6       /* buceando hacia atrás */
#define PV_WATER_UP 1       /* nadando hacia arriba */
#define PV_WATER_DOWN   4       /* nadando hacia abajo */
#define PV_TURN     18      /* giro 12 */
#define PV_FAST_TURN    40      /* giro rápido */
#define PV_LOOK     10      /* mirar arriba/abajo */

/* motion */

#define ACT_FORWARD     0x0001
#define ACT_BACK        0x0002
#define ACT_LEFT        0x0004
#define ACT_RIGHT       0x0008
#define ACT_UP          0x0010
#define ACT_DOWN        0x0020
#define ACT_FAST        0x0100
#define ACT_LOOKUP      0x0200
#define ACT_LOOKDOWN    0x0400

#define ACT_MASK        (ACT_FORWARD|ACT_BACK|ACT_LEFT|ACT_RIGHT)

/* externos */

extern int _snd_left_step;
extern int _snd_right_step;
extern int _snd_wleft_step;
extern int _snd_wright_step;
extern int _snd_water;
extern int _snd_underwater;
extern int _snd_player_dead;
extern int _snd_pump;
extern int _snd_console;
extern int _snd_water_go;


/* sonidos aleatorios */

struct rnd_sound
{
    int num;
    int sounds[10];
};

extern struct rnd_sound _over_water_sounds;
extern struct rnd_sound _under_water_sounds;


/** prototipos **/

void LoadRandomSound(struct rnd_sound * q, char * sound);
void StartRandomSound(struct rnd_sound * q);

void SetMessage(char * msg_0, char * msg_1, char * msg_2, char * msg3,
    int seconds);

int  DrawNewFrame(void);
void Motion(int * px, int * py, int * pz, int * pa);
void TimeEvents(int y);
void Objects(void);
void PlayerDrawMask(void);

void PlayerStartup(void);
void PlayerShutdown(void);
