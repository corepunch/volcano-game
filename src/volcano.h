#ifndef VOLCANO_H
#define VOLCANO_H
#include <SDL.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define GAME_WIDTH 1366
#define GAME_HEIGHT 768
#define MAX_LEVEL_OBJECTS 4096
#define MAX_TRAFFIC_POINTS 4096
#define PATH_CAPACITY 2048
#define MAX_MAP_VISUALS 1024

typedef struct { float x,y; } Vec2;
typedef struct { float dx,dy,u,v,cu,cv,w,h; } SpriteFrame;
typedef struct Animation {
    char name[128], texture[PATH_CAPACITY];
    SpriteFrame *frames;
    int frame_count;
    float fps;
} Animation;
typedef struct {
    uint32_t id,type,layer,link_id;
    char name[512],extra[4096];
    Vec2 auxiliary;
    float x,y,rotation,scale_x,scale_y;
    char params[4096];
} LevelObject;
typedef struct {
    uint32_t version,max_id;
    char background[128];
    LevelObject objects[MAX_LEVEL_OBJECTS];
    int object_count;
    Vec2 traffic[MAX_TRAFFIC_POINTS];
    int traffic_count;
    size_t trailing_bytes;
} Level;
typedef enum { MAP_FOAM, MAP_SPRITE, MAP_SPARKLES, MAP_INSECTS, MAP_MOTES } MapVisualKind;
typedef enum { MAP_GROUND, MAP_WORLD, MAP_OVERLAY } MapVisualStage;
typedef struct {
    MapVisualKind kind;
    MapVisualStage stage;
    uint32_t seed;
    Vec2 p,extent,pivot;
    SDL_Rect region;
    char image[PATH_CAPACITY],animation[128];
    float rate,speed,wind,scale_x,scale_y,rotation,opacity;
    int count;
} MapVisual;
typedef struct { MapVisual visuals[MAX_MAP_VISUALS];int visual_count; } MapScene;
extern char asset_root[PATH_CAPACITY];
extern SDL_Renderer *renderer;
extern char last_error[1024];

bool resources_init(void);
void resources_destroy(void);
Animation *animation_find(const char *name);
int animation_count(void);
bool draw_sprite(const char *name,float x,float y,double time,int frame,float alpha);
bool draw_sprite_rotated(const char *name,float x,float y,double time,int frame,float alpha,float angle);
bool draw_sprite_shadow(const char *name,float x,float y,double time,int frame,float alpha,float angle,float cast);
bool draw_sprite_tinted(const char *name,float x,float y,double time,int frame,SDL_Color tint);
bool draw_image(const char *relative,float x,float y,float w,float h,const char *alpha);
bool draw_image_file(const char *path,float x,float y,float w,float h);
bool image_file_size(const char *path,int *w,int *h);
bool draw_map_sprite(const char *path,SDL_Rect region,Vec2 pivot,Vec2 position,Vec2 scale,float bend,float angle,float opacity);
bool draw_map_animation(const char *name,Vec2 position,double time,float scale,float angle,float opacity);
void draw_glint(float x,float y,float radius,float opacity);
/* Type-5 custom map records; unknown retail controllers are preserved/ignored. */
bool map_visual_parse(MapScene *scene,const LevelObject *raw,const char *level_path,int *index);
void map_visual_draw(const MapVisual *visual,double time);
void map_scene_draw(const MapScene *scene,MapVisualStage stage,double time);
/* Retail GUI UVs and rectangles both use bottom-left coordinates. */
bool draw_atlas(const char *relative,SDL_FRect rect,SDL_FRect uv);
/* Original fire atlas (32 frames) and smoke, centered in world coordinates. */
bool draw_particle(bool smoke,int frame,float x,float y,float diameter,float angle,SDL_Color tint);
bool draw_particle_stretched(bool smoke,int frame,float x,float y,float w,float h,float angle,SDL_Color tint);
/* CC0 log-fire layers (0..79) and smoke variants (0..3), with curved updraft. */
bool draw_log_particle(bool smoke,int frame,float x,float y,float w,float h,float bend,SDL_Color tint);
/* Soft additive light for fire beds and ember trails, in world coordinates. */
bool draw_particle_glow(float x,float y,float w,float h,SDL_Color tint);
/* White foam geometry: feathered flecks or broken surface rings. */
bool draw_foam_particle(float x,float y,float w,float h,float angle,Uint8 opacity,bool ring);
void draw_font(const char *font,const char *text,float x,float y,int size,SDL_Color color);
float text_width(const char *font,const char *text,int size);
void draw_text(const char *text,float x,float y,int size,SDL_Color color);
void draw_rect(float x,float y,float w,float h,SDL_Color color);
void draw_line(float x,float y,float xx,float yy,SDL_Color color);
bool renderer_screenshot(const char *path);
bool resources_selftest(void);
/* Strict UTF-8 / BOM UTF-16 text decoding; caller frees returned buffer. */
char *read_resource_text(const char *path,size_t *length);
/* Original .lvl reader: preserves raw object IDs/types/parameters/transforms. */
bool level_load(const char *path,Level *level,char *error,size_t error_size);
bool level_selftest(const char *root);
/* Read a key from original semicolon/newline-separated object parameters. */
bool level_param(const char *params,const char *key,char *out,size_t size);
bool campaign_load(const char *root);
int campaign_index(int number);
int campaign_number(int index);

bool game_init(int index);
/* Custom binary, with optional PATH.rules mission settings. */
bool game_init_file(const char *path);
void game_destroy(void);
void game_update(double dt);
void game_draw(void);
void game_click(float x,float y);
void game_pointer(float x,float y,bool down);
void game_key(SDL_Keycode key);
bool game_selftest(void);
int game_level_index(void);
#endif
