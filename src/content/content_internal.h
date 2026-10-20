#ifndef VOLCANO_CONTENT_INTERNAL_H
#define VOLCANO_CONTENT_INTERNAL_H
/* Contract between the content registry, the mission loader and the retail importer. */
#include "content.h"
#include "../core/xml.h"

typedef const Archetype *(*ArchetypeLookup)(const char *id,void *context);
bool archetype_parse(const XmlDoc *doc,const XmlNode *node,Arena *arena,ArchetypeLookup lookup,void *context,
                     Archetype *out,const char **bonus_id);
bool content_stock(const XmlDoc *doc,const XmlNode *node,Stock *out);
bool mission_settings(const XmlDoc *doc,const XmlNode *node,const Mission *defaults,Mission *out);
const Mission *campaign_defaults(void);
bool level_spec_reserve(LevelSpec *spec,int placements,int traffic,int visuals);

/* ---- Retail Rescue Team 5 level binaries (version 301), read losslessly. */
typedef struct {
    uint32_t id,type,layer,link_id;
    const char *name,*params,*extra;
    Vec2 p,auxiliary;
    float rotation,scale_x,scale_y;
} RetailObject;
typedef struct {
    Arena arena;
    uint32_t version,max_id;
    const char *background;
    RetailObject *objects;int object_count;
    Vec2 *traffic;int traffic_count;
} RetailLevel;
bool retail_level_read(const char *path,RetailLevel *out);
void retail_level_free(RetailLevel *level);
/* Value for KEY in semicolon/newline-separated "key=value" parameter text. */
bool retail_param(const char *params,const char *key,char *out,size_t size);
#endif
