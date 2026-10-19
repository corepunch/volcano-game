#include "game_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdarg.h>


float game_distance(Vec2 a,Vec2 b) { return hypotf(a.x-b.x,a.y-b.y); }

SDL_Color game_color(int r,int g,int b,int a) { return (SDL_Color){r,g,b,a}; }

bool game_fail(const char *format,...)
{
    va_list args;va_start(args,format);vsnprintf(last_error,1024,format,args);va_end(args);return false;
}

void game_announce(Game *g, const char *text) { snprintf(g->message,sizeof(g->message),"%s",text);g->message_time=4; }

float game_particle_random(uint32_t seed)
{
    seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    return (seed>>8)*(1.f/16777216.f);
}
