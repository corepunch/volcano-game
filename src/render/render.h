#ifndef VOLCANO_RENDER_H
#define VOLCANO_RENDER_H
/* SDL presentation: primitives, map effects, the world view and the HUD.
 * Reads simulation state; never changes it (input does that, via game/). */
#include <SDL.h>
#include "../sim/world.h"
#include "../assets/assets.h"

extern SDL_Renderer *renderer;

void render_shutdown(void);
SDL_Color game_color(int r,int g,int b,int a);

/* Primitives. World coordinates have a bottom-left origin. */
bool draw_sprite(const char *name,float x,float y,double time,int frame,float alpha);
bool draw_sprite_rotated(const char *name,float x,float y,double time,int frame,float alpha,float angle);
bool draw_sprite_tinted(const char *name,float x,float y,double time,int frame,SDL_Color tint);
/* Image from the asset mounts (with optional retail JPEG alpha mask), or from a file path. */
bool draw_asset_image(const char *relative,float x,float y,float w,float h,const char *alpha);
bool draw_image_file(const char *path,float x,float y,float w,float h);
/* Retail GUI UVs and rectangles both use bottom-left coordinates. */
bool draw_atlas(const char *relative,SDL_FRect rect,SDL_FRect uv);
/* Catalog sprite drawn as geometry: scaled, rotated, canopy bent by BEND. */
bool draw_map_sprite(const char *name,double time,Vec2 position,Vec2 scale,float bend,float angle,float opacity);
void draw_glint(float x,float y,float radius,float opacity);
bool draw_particle(bool smoke,int frame,float x,float y,float diameter,float angle,SDL_Color tint);
bool draw_particle_stretched(bool smoke,int frame,float x,float y,float w,float h,float angle,SDL_Color tint);
/* CC0 log-fire layers (0..79) and smoke variants (0..3), with curved updraft. */
bool draw_log_particle(bool smoke,int frame,float x,float y,float w,float h,float bend,SDL_Color tint);
bool draw_particle_glow(float x,float y,float w,float h,SDL_Color tint);
bool draw_foam_particle(float x,float y,float w,float h,float angle,Uint8 opacity,bool ring);
void draw_font(const char *font,const char *text,float x,float y,int size,SDL_Color color);
float text_width(const char *font,const char *text,int size);
void draw_rect(float x,float y,float w,float h,SDL_Color color);
void draw_line(float x,float y,float xx,float yy,SDL_Color color);
bool renderer_screenshot(const char *path);

/* effects.c: mission map effects (foam, sparkles, insects, motes, sprites). */
void map_visual_draw(const MapVisual *visual,double time);
void map_scene_draw(const Game *g,MapVisualStage stage);

/* world_view.c */
bool game_on_roof(const Entity *e);
VisualPose game_visual_pose(const Game *g,const Entity *e);
int game_depth_compare(const Game *g,const void *a,const void *b);
void game_paint_log_particles(const Game *g,const Entity *e);
void game_render(Game *g);

/* hud.c: HUD, menus, tooltips and picking. Build menu options are menu slots
 * 0..3; option 4 is demolition. */
enum { BUILD_OPTION_DEMOLISH=4 };
bool game_inside(const Game *g,float x,float y,float w,float h);
int game_hud_button(const Game *g);
int game_hovered_object(const Game *g);
Vec2 game_build_button_position(const Game *g,int option);
int game_build_button_at(const Game *g);
void game_draw_hud(Game *g);
void game_draw_context(Game *g);
#endif
