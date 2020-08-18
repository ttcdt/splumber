/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

extern int screen_offset;

void * SeekCache  (char * name);
void AddCache     (char * name, void * data);
long RANDOM   (int range);
void RANDOMIZE    (long seed);
void SuppStartup  (void);
void SuppShutdown (void);
