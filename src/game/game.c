/* Session: startup, the active world and its lifecycle. */
#include "game_internal.h"
#include <stdio.h>
#include <string.h>

static Game *current_game;
static bool prefer_hd;

Game *game_current(void) { return current_game; }

bool game_startup(const GameConfig *config)
{
    vfs_reset();
    for (int i=0;i<config->mount_count;i++) if (!vfs_mount(config->mounts[i])) return false;
    prefer_hd=config->hd;
    return content_load(config->content) && assets_load_base();
}

void game_shutdown(void)
{
    game_destroy();assets_destroy();content_destroy();render_shutdown();
}

void game_destroy(void) { game_world_free(current_game);current_game=NULL; }

static bool load_level_art(const LevelSpec *spec)
{
    return assets_load_level(spec->packs,spec->pack_count,spec->sprite_files,spec->sprite_file_count);
}

/* HD sessions use NAME@2x.EXT beside a mission background when it exists. */
static void choose_background(LevelSpec *spec)
{
    if (!prefer_hd || spec->retail_background) return;
    char hd[PATH_CAPACITY];const char *dot=strrchr(spec->background,'.');
    if (!dot || snprintf(hd,sizeof hd,"%.*s@2x%s",(int)(dot-spec->background),spec->background,dot)>=(int)sizeof hd) return;
    if (file_exists(hd)) text_copy(spec->background,sizeof spec->background,hd);
}

static bool start(LevelSpec *spec)
{
    choose_background(spec);
    Game *candidate=load_level_art(spec)?game_world_create(spec):NULL;
    if (!candidate) {
        char reason[sizeof last_error];memcpy(reason,last_error,sizeof reason);
        level_spec_free(spec);
        /* Keep the running level drawable: restore its art layer. */
        if (current_game) load_level_art(&current_game->spec);else assets_clear_level();
        error_set("%s",reason);return false;
    }
    game_destroy();current_game=candidate;return true;
}

bool game_init(int source_index)
{
    LevelSpec spec;
    return retail_level_load(source_index,&spec) && start(&spec);
}

bool game_init_campaign(int number)
{
    if (!campaign_load()) return false;
    int index=campaign_index(number);
    return index>=0?game_init(index):error_set("Campaign level must be 1..50");
}

bool game_init_file(const char *path)
{
    LevelSpec spec;
    if (!path || !*path) return error_set("Missing mission path");
    return mission_load(path,&spec) && start(&spec);
}

/* Apply lifecycle requests only after the input callback has returned. */
static void apply_command(GameCommand command)
{
    if (!current_game || command==GAME_COMMAND_NONE) return;
    const LevelSpec *spec=&current_game->spec;
    if (command==GAME_COMMAND_NEXT && spec->source_index>=0) { game_init_campaign(spec->campaign+1);return; }
    char path[PATH_CAPACITY];text_copy(path,sizeof path,spec->path);
    bool ok=spec->source_index<0?game_init_file(path):game_init(spec->source_index);
    if (!ok && command==GAME_COMMAND_RESTART) { game_announce(current_game,last_error);error_clear(); }
}

void game_update(double seconds) { game_advance(current_game,seconds); }
void game_draw(void) { game_render(current_game); }
void game_pointer(float x,float y,bool down) { game_set_pointer(current_game,x,y,down); }
void game_click(float x,float y) { apply_command(game_handle_click(current_game,x,y)); }
void game_key(SDL_Keycode key) { apply_command(game_handle_key(current_game,key)); }
int game_level_index(void) { return current_game?current_game->spec.source_index:0; }
