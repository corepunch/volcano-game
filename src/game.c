#include "game_internal.h"
#include <stdlib.h>
#include <stdio.h>

/* The application owns one world. Candidate loading never changes this pointer. */
static Game *current_game;
Game *game_current(void) { return current_game; }
void game_destroy(void) { free(current_game);current_game=NULL; }
static bool init_level(int index,const char *path)
{
    Game *candidate=calloc(1,sizeof *candidate);
    if (!candidate) return game_fail("Cannot allocate gameplay state");
    if (!game_load_level(candidate,index,path)) {free(candidate);return false;}
    game_destroy();current_game=candidate;return true;
}
bool game_init(int index) { return init_level(index,NULL); }
bool game_init_file(const char *path)
{
    return path && *path?init_level(-1,path):game_fail("Missing custom level path");
}
/* Apply lifecycle requests only after the input callback has returned. */
static void apply_command(GameCommand command)
{
    if (!current_game || command==GAME_COMMAND_NONE) return;
    if (command==GAME_COMMAND_NEXT && !*current_game->custom_path) {
        game_init(campaign_index(current_game->campaign+1));return;
    }
    char path[PATH_CAPACITY];snprintf(path,sizeof path,"%s",current_game->custom_path);
    bool ok=*path?game_init_file(path):game_init(current_game->index);
    if (!ok && command==GAME_COMMAND_RESTART) {
        game_announce(current_game,last_error);last_error[0]=0;
    }
}
void game_update(double dt) { game_tick(current_game,dt); }
void game_draw(void) { game_render(current_game); }
void game_pointer(float x,float y,bool down) { game_set_pointer(current_game,x,y,down); }
void game_click(float x,float y) { apply_command(game_handle_click(current_game,x,y)); }
void game_key(SDL_Keycode key) { apply_command(game_handle_key(current_game,key)); }
int game_level_index(void) { return current_game?current_game->index:0; }
