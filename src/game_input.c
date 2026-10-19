#include "game_internal.h"

void game_set_pointer(Game *g, float x,float y,bool down)
{
    if (!g) return;
    g->pointer_x=x;g->pointer_y=GAME_HEIGHT-y;g->pointer_down=down;
}

GameCommand game_handle_click(Game *g, float x,float y)
{
    if (!g || !g->ready) return GAME_COMMAND_NONE;
    game_set_pointer(g, x,y,false);
    int button=game_hud_button(g);
    if (g->won) {
        if (button==1) return GAME_COMMAND_NEXT;
        else if (button==2) return GAME_COMMAND_RESTART;
        return GAME_COMMAND_NONE;
    }
    if (button==0 || button==1) { g->paused=!g->paused;g->build_menu=-1;return GAME_COMMAND_NONE; }
    if (button==2) return GAME_COMMAND_RESTART;
    if (g->paused) return GAME_COMMAND_NONE;
    if (game_inside(g, 0,668,446,100) || game_inside(g, 1010,671,356,97)) return GAME_COMMAND_NONE;
    if (g->build_menu>=0) {
        int option=game_build_button_at(g);
        if (option==4) {
            Entity *building=&g->objects[g->build_menu];
            if (game_output_busy(g, building)) { game_announce(g, "Wait for the supply delivery before demolishing.");return GAME_COMMAND_NONE; }
            building->action=DEMOLISH;building->duration=5;
            if (game_dispatch(g, g->build_menu,false)) g->build_menu=-1;
            return GAME_COMMAND_NONE;
        }
        if (option>=0) {
            Entity *lot=&g->objects[g->build_menu];
            if (!game_allowed_building(g, option)) {game_announce(g, "Burger Cafes cannot be built on this level. Repair the piers and fish for food.");return GAME_COMMAND_NONE;}
            if (g->wood<game_buildings[option].cost) { game_announce(g, "Not enough wood for this building.");return GAME_COMMAND_NONE; }
            lot->building=option;lot->duration=5;
            if (game_dispatch(g, g->build_menu,false)) g->build_menu=-1;
            return GAME_COMMAND_NONE;
        }
        g->build_menu=-1;return GAME_COMMAND_NONE;
    }
    int best=game_hovered_object(g);
    if (best<0) return GAME_COMMAND_NONE;
    Entity *e=&g->objects[best];
    if (g->early && e->kind==BUILDING && e->operational && !e->assigned) {g->build_menu=best;return GAME_COMMAND_NONE;}
    if (e->kind==BUILD_LOT && !e->assigned) { g->build_menu=best;return GAME_COMMAND_NONE; }
    if (!game_job(g, e) || (e->done && e->kind!=BUILDING) || e->assigned) return GAME_COMMAND_NONE;
    if (e->queued) { e->queued=false;return GAME_COMMAND_NONE; }
    if (!game_dispatch(g, best,false) && game_reachable(g, best)) {
        e->queued=true;e->queue_order=++g->next_order;
        char text[192];game_waiting_detail(g, best,text,sizeof text);game_announce(g, text);
    }
    return GAME_COMMAND_NONE;
}

GameCommand game_handle_key(Game *g, SDL_Keycode key)
{
    if (!g) return GAME_COMMAND_NONE;
    if (g->won && (key==SDLK_RETURN || key==SDLK_n)) {
        return GAME_COMMAND_NEXT;
    }
    if (g->won && key!=SDLK_r) return GAME_COMMAND_NONE;
    if (key==SDLK_ESCAPE && g->build_menu>=0) { g->build_menu=-1;return GAME_COMMAND_NONE; }
    if (key==SDLK_SPACE || key==SDLK_p || key==SDLK_ESCAPE) { g->paused=!g->paused;g->build_menu=-1; }
    else if (key==SDLK_r) return GAME_COMMAND_RESTART;
    else if (key==SDLK_F1) g->debug=!g->debug;
    return GAME_COMMAND_NONE;
}
