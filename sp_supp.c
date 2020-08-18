/*

    Space Plumber

    Angel Ortega <angel@triptico.com>

    This software is released into the public domain.
    NO WARRANTY. See file LICENSE for details.

*/

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include "qdgdf_video.h"
#include "qdgdf_audio.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#include "sp_types.h"
#include "sp_supp.h"


/* file cache */

#define FILE_CACHE_NAME_SIZE 60
struct file_cache {
    char name[FILE_CACHE_NAME_SIZE];
    void *data;
    struct file_cache *next;
};

static struct file_cache *_file_cache = NULL;


static unsigned int _random = 0;

int screen_offset = 0;


/********************
    Code
*********************/

/** cache **/

void *SeekCache(char *name)
{
    struct file_cache *cache;

    cache = _file_cache;

    while (cache != NULL) {
        if (strcmp(name, cache->name) == 0)
            return (cache->data);

        cache = cache->next;
    }

    return (NULL);
}


void AddCache(char *name, void *data)
{
    struct file_cache *c;

    c = (struct file_cache *) qdgdfv_malloc(sizeof(struct file_cache));

    strcpy(c->name, name);
    c->data = data;
    c->next = _file_cache;

    _file_cache = c;
}


unsigned long sp_random(void)
{
    _random = (_random * 58321) + 11113;

    return (_random >> 16);
}


void sp_randomize(int seed)
{
    _random = seed;
}


long RANDOM(int range)
{
#ifdef STD_RANDOM
    return (random() % (range));
#else
    return (sp_random() % (range));
#endif
}


void RANDOMIZE(long seed)
{
#ifdef STD_RANDOM
    srandom(seed);
#else
    sp_randomize(seed);
#endif
}


void SuppStartup(void)
{
    qdgdfv_logger("Space Plumber (qdgdf version)", "boot...");
}


void SuppShutdown(void)
{
}
