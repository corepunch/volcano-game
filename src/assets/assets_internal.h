#ifndef VOLCANO_ASSETS_INTERNAL_H
#define VOLCANO_ASSETS_INTERNAL_H
/* Contract between the catalog and its declaration importers. */
#include "assets.h"

/* Where an importer's relative image paths resolve: PACK first when set,
 * otherwise (and as a fallback) the VFS mounts. */
typedef struct { const char *pack;int source; } DeclarationScope;

/* Creates or replaces NAME. Same-source duplicates are rejected. */
Sprite *catalog_new_sprite(const char *name,int source);
bool catalog_add_frame(Sprite *sprite,SpriteFrame frame);
bool catalog_set_texture(Sprite *sprite,const char *path);
Font *catalog_new_font(const char *name);
bool asset_resolve(const DeclarationScope *scope,const char *relative,char *out,size_t size);

/* Retail atlas/font declarations: a data grammar, never a script interpreter. */
bool retail_declarations_parse(const char *text,size_t size,const char *path,bool fonts,const DeclarationScope *scope);
bool retail_declarations_selftest(void);
/* Decodes UTF-8 (BOM optional) or BOM-marked UTF-16 text into UTF-8; caller frees. */
char *declaration_text(const char *path,size_t *length);
#endif
