#ifndef VOLCANO_GAME_INTERNAL_H
#define VOLCANO_GAME_INTERNAL_H
/* Everything inside the game: session API plus every layer's contract.
 * Tests include this to drive the real modules. */
#include "game.h"
#include "../render/render.h"

Game *game_current(void);

/* input.c: lifecycle requests are applied by game.c after input returns. */
typedef enum { GAME_COMMAND_NONE,GAME_COMMAND_RESTART,GAME_COMMAND_NEXT } GameCommand;
void game_set_pointer(Game *g,float x,float y,bool down);
GameCommand game_handle_click(Game *g,float x,float y);
GameCommand game_handle_key(Game *g,SDL_Keycode key);
#endif
