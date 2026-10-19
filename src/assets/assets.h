#ifndef VOLCANO_ASSETS_H
#define VOLCANO_ASSETS_H
/* Sprite and font catalog. Pure data, no SDL: the simulation measures
 * footprints from it and the renderer draws from it.
 *
 * Sources, lowest precedence first:
 *   base layer  - every VFS mount, bottom to top:
 *                 texture/lazy/NAME.lua      retail declarations (imported)
 *                 sprites/NAME.sprites.xml native declarations
 *   level layer - packs and sprite files a mission declares; replaced per level.
 * A later source replaces an earlier sprite of the same name. Within one
 * mount or file a duplicate name is an error. */
#include "../core/core.h"

typedef struct {
    float dx,dy,u,v,cu,cv,w,h;          /* Anchor offset, normalized bottom-left UVs, world size. */
    float body_dx,body_dy,body_w,body_h; /* Optional bounds excluding baked shadow padding. */
} SpriteFrame;
typedef struct {
    char name[128];
    const char *texture;                /* Absolute image path. */
    SpriteFrame *frames;
    int frame_count,source;             /* Source: mount index, or -1 for the level layer. */
    float fps;
} Sprite;
typedef struct { uint32_t code;int x,y,w,h,bx,by,advance; } Glyph;
typedef struct { char name[128],texture[PATH_CAPACITY];Glyph *glyphs;int count,size; } Font;

static inline SpriteFrame sprite_body_frame(SpriteFrame f)
{
    if (f.body_w>0) { f.dx=f.body_dx;f.dy=f.body_dy;f.w=f.body_w;f.h=f.body_h; }
    return f;
}

bool assets_load_base(void);
/* Level layer: pack folders (laid out like mounts) and native sprite files. */
bool assets_load_level(const char *const *packs,int pack_count,const char *const *files,int file_count);
void assets_clear_level(void);
void assets_destroy(void);

const Sprite *sprite_find(const char *name);
int sprite_count(void);
const Sprite *sprite_at(int index);   /* Base layer then level layer. */
const Font *font_find(const char *name);
int font_count(void);
const Font *font_at(int index);

/* Image header query (no decoding). */
bool image_size(const char *path,int *w,int *h);
/* Decodes RGBA pixels; free with image_release(). */
unsigned char *image_decode(const char *path,int *w,int *h);
unsigned char *image_decode_gray(const char *path,int *w,int *h);
void image_release(void *pixels);

/* Parser and catalog checks that need no retail data. */
bool assets_selftest(void);
#endif
