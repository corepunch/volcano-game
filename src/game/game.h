#ifndef VOLCANO_GAME_H
#define VOLCANO_GAME_H
/* Application-facing API. The session owns one active world; loading builds
 * and validates a fresh candidate and replaces the world only on success,
 * so a failed load leaves the running level untouched. */
#include <SDL.h>
#include "../core/core.h"

typedef struct {
    const char *content;            /* Folder holding archetypes.xml and campaign.xml. */
    const char *mounts[VFS_MAX_MOUNTS];
    int mount_count;                /* Asset folders, lowest precedence first. */
    bool hd;                        /* Prefer NAME@2x backgrounds when present. */
} GameConfig;

/* Mounts assets and loads content and the sprite catalog. Needs `renderer` set. */
bool game_startup(const GameConfig *config);
void game_shutdown(void);

bool game_init(int source_index);          /* Original level file number. */
bool game_init_campaign(int number);       /* Original campaign position 1-50. */
bool game_init_file(const char *path);     /* Native mission file. */
void game_destroy(void);

void game_update(double seconds);          /* Advances the fixed-step simulation. */
void game_draw(void);
void game_click(float x,float y);
void game_pointer(float x,float y,bool down);
void game_key(SDL_Keycode key);
int game_level_index(void);
int game_canvas_width(void);
int game_canvas_height(void);
#endif
