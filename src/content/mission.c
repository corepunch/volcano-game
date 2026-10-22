/* Native mission files (XML), the project's own level format.
 *
 * <mission format="volcano-mission-1" title="Library Rescue" background="background.png">
 *   <settings rules="rescue" food="40" people="10"/>
 *   <pack path="../../assets/kenney-characters"/>
 *   <sprites path="props.sprites.xml"/>
 *   <archetype id="field-kitchen" extends="cafe" title="Field kitchen" sprite="..." damaged="..."/>
 *   <traffic>210,248 237.5,245.5 ...</traffic>
 *   <object id="30" type="field-kitchen" x="245" y="378"/>
 *   <effect kind="motes" x="690" y="438" width="130" height="70" count="5"/>
 * </mission>
 *
 * Paths are relative to the mission file. Coordinates are world units with a
 * bottom-left origin on the 1366x768 playfield. */
#include "content_internal.h"
#include "../assets/assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_PLACEMENTS=4096,MAX_POINTS=4096,MAX_VISUALS=1024 };
static const char FORMAT[]="volcano-mission-1";

bool level_spec_reserve(LevelSpec *spec,int placements,int traffic,int visuals)
{
    spec->placements=arena_alloc(&spec->arena,(size_t)(placements?placements:1)*sizeof *spec->placements);
    spec->traffic=arena_alloc(&spec->arena,(size_t)(traffic?traffic:1)*sizeof *spec->traffic);
    spec->visuals=arena_alloc(&spec->arena,(size_t)(visuals?visuals:1)*sizeof *spec->visuals);
    return spec->placements && spec->traffic && spec->visuals;
}

void level_spec_free(LevelSpec *spec) { arena_free(&spec->arena);memset(spec,0,sizeof *spec); }

const Placement *level_placement(const LevelSpec *spec,uint32_t id)
{
    for (int i=0;i<spec->placement_count;i++) if (spec->placements[i].id==id) return &spec->placements[i];
    return NULL;
}

typedef struct { LevelSpec *spec;const XmlDoc *doc;char folder[PATH_CAPACITY]; } Loader;

static const Archetype *local_lookup(const char *id,void *context)
{
    const LevelSpec *spec=context;
    for (int i=0;i<spec->local_count;i++) if (!strcmp(spec->local[i]->id,id)) return spec->local[i];
    return archetype_find(id);
}

static const char *relative_path(Loader *l,const XmlNode *node,const char *key)
{
    const char *text;char path[PATH_CAPACITY];
    if (!xml_require(l->doc,node,key,&text)) return NULL;
    if (text[0]=='/' || !path_join(path,sizeof path,l->folder,text)) { xml_fail(l->doc,node,"%s must be a relative path",key);return NULL; }
    return arena_strdup(&l->spec->arena,path);
}

static bool load_archetypes(Loader *l)
{
    int count=xml_count(l->doc->root,"archetype");
    l->spec->local=arena_alloc(&l->spec->arena,(size_t)(count?count:1)*sizeof *l->spec->local);
    if (!l->spec->local) return false;
    const char *bonus_ids[64];
    if (count>64) return xml_fail(l->doc,l->doc->root,"declares more than 64 archetypes");
    XML_EACH(node,l->doc->root,"archetype") {
        Archetype *a=arena_alloc(&l->spec->arena,sizeof *a);
        if (!a || !archetype_parse(l->doc,node,&l->spec->arena,local_lookup,l->spec,a,&bonus_ids[l->spec->local_count])) return false;
        if (local_lookup(a->id,l->spec)) return xml_fail(l->doc,node,"archetype \"%s\" is already defined",a->id);
        l->spec->local[l->spec->local_count++]=a;
    }
    for (int i=0;i<l->spec->local_count;i++) if (bonus_ids[i]) {
        const Archetype *bonus=local_lookup(bonus_ids[i],l->spec);
        if (!bonus || bonus->kind!=CLASS_PICKUP) return error_set("%s: archetype %s: bonus \"%s\" is not a pickup",l->doc->source,l->spec->local[i]->id,bonus_ids[i]);
        l->spec->local[i]->bonus=bonus;
    }
    return true;
}

static int placement_index(const LevelSpec *spec,uint32_t id)
{
    for (int i=0;i<spec->placement_count;i++) if (spec->placements[i].id==id) return i;
    return -1;
}

static bool relation(Loader *l,const XmlNode *node,const char *key,const ArchetypeClass *allowed,int count,int *out)
{
    uint32_t id;
    if (!xml_uint(l->doc,node,key,0,&id)) return false;
    if (!id) return true;
    int index=placement_index(l->spec,id);
    if (index<0) return xml_fail(l->doc,node,"%s=\"%u\" names no object",key,id);
    bool valid=false;
    for (int i=0;i<count;i++) valid|=l->spec->placements[index].type->kind==allowed[i];
    if (!valid) return xml_fail(l->doc,node,"%s=\"%u\" names an object of the wrong kind",key,id);
    *out=index;return true;
}

static bool load_object(Loader *l,const XmlNode *node,Placement *p)
{
    static const char *attributes[]={"id","type","x","y","layer","sprite","variant","operational","burning","floating",
        "roof","transport","producer","gate","service","support","workpoint","home","corridor","speed",NULL};
    const char *type_id,*transport;float corridor[3];
    memset(p,0,sizeof *p);p->producer=p->gate=p->service=p->support=-1;
    if (!xml_only(l->doc,node,attributes,NULL) || !xml_uint(l->doc,node,"id",0,&p->id) || !xml_require(l->doc,node,"type",&type_id))
        return false;
    if (!p->id) return xml_fail(l->doc,node,"needs a positive id");
    if (placement_index(l->spec,p->id)>=0) return xml_fail(l->doc,node,"duplicate id %u",p->id);
    if (!(p->type=local_lookup(type_id,l->spec))) return xml_fail(l->doc,node,"unknown type \"%s\"",type_id);
    ArchetypeClass k=p->type->kind;
    bool actor=k==CLASS_CIVILIAN || k==CLASS_WORKER || k==CLASS_ANIMAL || k==CLASS_VEHICLE;
    int layer;
    if (!xml_float(l->doc,node,"x",0,-10000,10000,&p->p.x) || !xml_float(l->doc,node,"y",0,-10000,10000,&p->p.y) ||
        !xml_int(l->doc,node,"layer",actor?6:5,0,1000,&layer) || !xml_int(l->doc,node,"variant",0,0,p->type->variants-1,&p->variant) ||
        !xml_bool(l->doc,node,"operational",false,&p->operational) || !xml_bool(l->doc,node,"burning",false,&p->burning) ||
        !xml_bool(l->doc,node,"floating",false,&p->floating) || !xml_bool(l->doc,node,"roof",false,&p->roof) ||
        !xml_float(l->doc,node,"speed",0,0,10000,&p->travel_speed)) return false;
    p->layer=(uint32_t)layer;p->ambient_start=p->p;
    if (p->burning && !p->type->burnable) return xml_fail(l->doc,node,"type %s cannot burn",p->type->id);
    const char *sprite=xml_attr(node,"sprite");
    if (sprite && !(p->sprite=arena_strdup(&l->spec->arena,sprite))) return false;
    if ((transport=xml_attr(node,"transport"))) {
        if (!strcmp(transport,"atv")) p->transport=TRANSPORT_ATV;
        else if (!strcmp(transport,"boat")) p->transport=TRANSPORT_BOAT;
        else if (strcmp(transport,"foot")) return xml_fail(l->doc,node,"transport must be foot, atv or boat");
    }
    if (xml_attr(node,"workpoint")) {
        if (!xml_floats(l->doc,node,"workpoint",&p->workpoint.x,2,true)) return false;
        p->has_workpoint=true;
    }
    if (xml_attr(node,"home")) {
        if (!xml_floats(l->doc,node,"home",&p->home.x,2,true)) return false;
        p->has_home=true;
    }
    if (xml_attr(node,"corridor")) {
        if (!xml_floats(l->doc,node,"corridor",corridor,3,true)) return false;
        p->corridor=vec2(corridor[0],corridor[1]);p->corridor_radius=corridor[2];p->has_corridor=true;
    }
    if (p->type->ambient && !p->travel_speed) p->travel_speed=p->type->ambient==AMBIENT_BIRD?110:8+hash_random(p->id)*6;
    return true;
}

static bool load_relations(Loader *l)
{
    static const ArchetypeClass producers[]={CLASS_PRODUCER,CLASS_LOT},valves[]={CLASS_VALVE},
        services[]={CLASS_SERVICE},houses[]={CLASS_DECOR};
    int i=0;
    XML_EACH(node,l->doc->root,"object") {
        Placement *p=&l->spec->placements[i++];
        if (!relation(l,node,"producer",producers,2,&p->producer) || !relation(l,node,"gate",valves,1,&p->gate) ||
            !relation(l,node,"service",services,1,&p->service) || !relation(l,node,"support",houses,1,&p->support)) return false;
        if (p->producer>=0 && p->type->kind!=CLASS_PICKUP) return xml_fail(l->doc,node,"only pickups have a producer");
        if (p->type->kind==CLASS_VEHICLE && (p->service<0 || !p->has_home)) return xml_fail(l->doc,node,"vehicle needs service and home");
    }
    return true;
}

static bool load_effect(Loader *l,const XmlNode *node,MapVisual *v)
{
    static const char *attributes[]={"kind","x","y","width","height","rate","speed","wind","opacity","count","scale-x",
        "scale-y","rotation","stage","sprite","fall","drift",NULL};
    static const char *kinds[]={"foam","waterfall","sprite","sparkles","insects","motes"};
    static const MapVisualKind types[]={MAP_FOAM,MAP_FOAM,MAP_SPRITE,MAP_SPARKLES,MAP_INSECTS,MAP_MOTES};
    const char *kind,*stage,*sprite;int index=-1;
    if (!xml_only(l->doc,node,attributes,NULL) || !xml_require(l->doc,node,"kind",&kind)) return false;
    for (int i=0;i<6;i++) if (!strcmp(kind,kinds[i])) index=i;
    if (index<0) return xml_fail(l->doc,node,"unknown kind \"%s\"",kind);
    memset(v,0,sizeof *v);v->kind=types[index];v->seed=(uint32_t)(l->spec->visual_count+1)*2654435761u;
    v->stage=v->kind==MAP_INSECTS || v->kind==MAP_MOTES?MAP_OVERLAY:MAP_GROUND;
    bool foam=v->kind==MAP_FOAM;float count;
    if (!xml_float(l->doc,node,"x",0,-10000,10000,&v->p.x) || !xml_float(l->doc,node,"y",0,-10000,10000,&v->p.y) ||
        !xml_float(l->doc,node,"width",32,1,600,&v->extent.x) || !xml_float(l->doc,node,"height",foam?28:40,1,600,&v->extent.y) ||
        !xml_float(l->doc,node,"rate",foam?26:6,.1f,180,&v->rate) || !xml_float(l->doc,node,"speed",1,.05f,6,&v->speed) ||
        !xml_float(l->doc,node,"wind",0,0,16,&v->wind) || !xml_float(l->doc,node,"opacity",1,0,1,&v->opacity) ||
        !xml_float(l->doc,node,"count",v->kind==MAP_INSECTS?3:8,1,32,&count) ||
        !xml_float(l->doc,node,"scale-x",1,.02f,5,&v->scale_x) || !xml_float(l->doc,node,"scale-y",1,.02f,5,&v->scale_y) ||
        !xml_float(l->doc,node,"rotation",0,-360,360,&v->rotation)) return false;
    if (count!=(int)count) return xml_fail(l->doc,node,"count must be an integer");
    v->count=(int)count;
    if (index==1) {
        float fall,drift;
        if (!xml_float(l->doc,node,"fall",72,16,220,&fall) || !xml_float(l->doc,node,"drift",0,-50,50,&drift)) return false;
        v->p.x+=drift;v->p.y-=fall;v->extent.y=28;
    }
    if ((stage=xml_attr(node,"stage"))) {
        if (!strcmp(stage,"ground")) v->stage=MAP_GROUND;
        else if (!strcmp(stage,"overlay")) v->stage=MAP_OVERLAY;
        else return xml_fail(l->doc,node,"stage must be ground or overlay (place world-depth art as a decor object)");
    }
    sprite=xml_attr(node,"sprite");
    if (v->kind==MAP_SPRITE && !sprite) return xml_fail(l->doc,node,"sprite effects need a sprite");
    if (v->kind==MAP_INSECTS && !sprite) sprite="butterfly";
    if (sprite && !(v->sprite=arena_strdup(&l->spec->arena,sprite))) return false;
    return true;
}

bool mission_load(const char *path,LevelSpec *out)
{
    memset(out,0,sizeof *out);
    XmlDoc doc;
    if (!xml_load(&doc,path)) return false;
    Loader l={out,&doc,""};path_directory(l.folder,sizeof l.folder,path);
    static const char *attributes[]={"format","title","background","retail-background","width","height",NULL};
    static const char *children[]={"settings","pack","sprites","archetype","traffic","object","effect",NULL};
    static const char *settings_attributes[]={"rules","food","wood","money","fuel","people","basins","clear-routes","crew-at-hq","offroad","buildable",NULL};
    const XmlNode *root=doc.root;const char *format,*title,*text;
    bool ok=!strcmp(root->name,"mission")?xml_only(&doc,root,attributes,children):xml_fail(&doc,root,"expected <mission> root");
    if (ok) ok=xml_require(&doc,root,"format",&format);
    if (ok && strcmp(format,FORMAT)) ok=xml_fail(&doc,root,"format must be \"%s\"",FORMAT);
    if (ok) {
        text_copy(out->path,sizeof out->path,path);out->source_index=-1;
        xml_string(&doc,root,"title","Custom level",&title);snprintf(out->title,sizeof out->title,"%s",title);
    }
    if (ok) ok=xml_int(&doc,root,"width",GAME_WIDTH,1024,4096,&out->width) &&
        xml_int(&doc,root,"height",GAME_HEIGHT,768,4096,&out->height);
    const XmlNode *settings=ok?xml_next(root,"settings",NULL):NULL;
    if (ok && (!settings || xml_count(root,"settings")>1)) ok=xml_fail(&doc,root,"needs exactly one <settings>");
    if (ok) {
        Mission defaults=*campaign_defaults();defaults.start=(Stock){{0}};defaults.rules=NULL;
        defaults.people_goal=defaults.basin_goal=0;defaults.crew_at_hq=false;defaults.offroad=true;
        ok=xml_only(&doc,settings,settings_attributes,NULL) && mission_settings(&doc,settings,&defaults,&out->mission);
    }
    if (ok) {
        bool image=xml_attr(root,"background")!=NULL,retail=xml_attr(root,"retail-background")!=NULL;
        if (image==retail) ok=xml_fail(&doc,root,"needs exactly one of background or retail-background");
        else if (image) {
            const char *file=relative_path(&l,root,"background");
            ok=file && text_copy(out->background,sizeof out->background,file);
        } else {
            text=xml_attr(root,"retail-background");out->retail_background=true;
            snprintf(out->background,sizeof out->background,"texture/back/%s_0.jpg",text);
            snprintf(out->background_alpha,sizeof out->background_alpha,"texture/back/%s_0.alpha",text);
        }
    }
    XML_EACH(node,root,"pack") {
        static const char *pack_attributes[]={"path",NULL};
        if (!ok) break;
        if (out->pack_count==8) { ok=xml_fail(&doc,node,"too many packs");break; }
        ok=xml_only(&doc,node,pack_attributes,NULL) && (out->packs[out->pack_count++]=relative_path(&l,node,"path"))!=NULL;
    }
    XML_EACH(node,root,"sprites") {
        static const char *sprite_attributes[]={"path",NULL};
        if (!ok) break;
        if (out->sprite_file_count==8) { ok=xml_fail(&doc,node,"too many sprite files");break; }
        ok=xml_only(&doc,node,sprite_attributes,NULL) && (out->sprite_files[out->sprite_file_count++]=relative_path(&l,node,"path"))!=NULL;
    }
    if (ok) ok=load_archetypes(&l);
    int objects=xml_count(root,"object"),visuals=xml_count(root,"effect"),points=0;
    XML_EACH(node,root,"traffic") for (const char *s=node->text;*s;s++) points+=*s==',';
    if (ok && (objects>MAX_PLACEMENTS || points>MAX_POINTS || visuals>MAX_VISUALS))
        ok=xml_fail(&doc,root,"exceeds %d objects, %d traffic points or %d effects",MAX_PLACEMENTS,MAX_POINTS,MAX_VISUALS);
    if (ok) ok=level_spec_reserve(out,objects,points,visuals);
    XML_EACH(node,root,"traffic") {
        static const char *traffic_attributes[]={"layer",NULL};
        Vec2 *list;int count;
        if (!ok) break;
        ok=xml_only(&doc,node,traffic_attributes,NULL) && xml_points(&doc,node,&out->arena,&list,&count);
        if (ok) { memcpy(out->traffic+out->traffic_count,list,(size_t)count*sizeof *list);out->traffic_count+=count; }
    }
    XML_EACH(node,root,"object") {
        if (!ok) break;
        if ((ok=load_object(&l,node,&out->placements[out->placement_count]))) out->placement_count++;
    }
    if (ok) ok=load_relations(&l);
    XML_EACH(node,root,"effect") {
        if (!ok) break;
        if ((ok=load_effect(&l,node,&out->visuals[out->visual_count]))) out->visual_count++;
    }
    xml_free(&doc);
    if (!ok) { char reason[sizeof last_error];memcpy(reason,last_error,sizeof reason);level_spec_free(out);error_set("%s",reason); }
    return ok;
}
