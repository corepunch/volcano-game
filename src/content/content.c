/* Content registry: archetypes, rulesets and campaign missions from
 * content/archetypes.xml and content/campaign.xml. */
#include "content_internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *const resource_names[RESOURCE_COUNT]={"food","wood","money","fuel"};

enum { MAX_ARCHETYPES=256,MAX_RULESETS=8,CAMPAIGN_MISSIONS=50 };
static Arena arena;
static Archetype *archetypes[MAX_ARCHETYPES];
static int archetype_count;
static Ruleset rulesets[MAX_RULESETS];
static int ruleset_count;
static Mission campaign[CAMPAIGN_MISSIONS+1];
static bool campaign_defined[CAMPAIGN_MISSIONS+1];
static Mission campaign_default;
static char directory[PATH_CAPACITY];

static const char *const class_names[CLASS_COUNT]={
    "decor","water","base","obstacle","pickup","civilian","worker","lot","producer","animal","valve","service","vehicle"};

const char *content_directory(void) { return directory; }

const Archetype *archetype_find(const char *id)
{
    for (int i=0;i<archetype_count;i++) if (!strcmp(archetypes[i]->id,id)) return archetypes[i];
    return NULL;
}

const Ruleset *ruleset_find(const char *id)
{
    for (int i=0;i<ruleset_count;i++) if (!strcmp(rulesets[i].id,id)) return &rulesets[i];
    return NULL;
}

bool archetype_is(const Archetype *a,const Archetype *ancestor)
{
    for (;a;a=a->base) if (a==ancestor) return true;
    return false;
}

int content_menu(const Archetype **out,int capacity)
{
    int count=0;
    for (int slot=0;slot<capacity;slot++) for (int i=0;i<archetype_count;i++)
        if (archetypes[i]->kind==CLASS_PRODUCER && archetypes[i]->menu_slot==slot) { out[count++]=archetypes[i];break; }
    return count;
}

bool mission_allows(const Mission *m,const Archetype *a)
{
    for (int i=0;i<m->buildable_count;i++) if (m->buildable[i]==a) return true;
    return false;
}

static int find_name(const char *const *names,int count,const char *name)
{
    for (int i=0;i<count;i++) if (!strcmp(names[i],name)) return i;
    return -1;
}

bool content_stock(const XmlDoc *doc,const XmlNode *node,Stock *out)
{
    static const char *keys[]={"food","wood","money","fuel",NULL};
    if (!xml_only(doc,node,keys,NULL)) return false;
    for (int r=0;r<RESOURCE_COUNT;r++)
        if (!xml_int(doc,node,resource_names[r],out->amount[r],0,1000000,&out->amount[r])) return false;
    return true;
}

static bool resource_attribute(const XmlDoc *doc,const XmlNode *node,const char *key,Resource *out)
{
    const char *text=xml_attr(node,key);
    if (!text) return true;
    int r=find_name(resource_names,RESOURCE_COUNT,text);
    if (r<0) return xml_fail(doc,node,"%s=\"%s\": expected food, wood, money or fuel",key,text);
    *out=(Resource)r;return true;
}

static bool copy_text(Arena *a,const XmlNode *node,const char *key,const char **field)
{
    const char *text=xml_attr(node,key);
    if (!text) return true;
    return (*field=arena_strdup(a,text))!=NULL;
}

static void class_defaults(Archetype *a)
{
    static const float durations[CLASS_COUNT]={
        [CLASS_OBSTACLE]=5,[CLASS_CIVILIAN]=.8f,[CLASS_ANIMAL]=1.2f,[CLASS_LOT]=5,[CLASS_PRODUCER]=5,[CLASS_SERVICE]=5,[CLASS_VALVE]=5};
    a->duration=durations[a->kind];
    a->approach=a->kind==CLASS_OBSTACLE || a->kind==CLASS_PRODUCER || a->kind==CLASS_LOT ||
        a->kind==CLASS_SERVICE || a->kind==CLASS_VALVE?APPROACH_FOOTPRINT:APPROACH_CENTER;
    a->directions=8;a->variants=1;a->opacity=1;a->cleared_frame=-1;a->menu_slot=-1;
    a->resource=a->produces=RESOURCE_NONE;
}

bool archetype_parse(const XmlDoc *doc,const XmlNode *node,Arena *a,ArchetypeLookup lookup,void *context,
                     Archetype *out,const char **bonus_id)
{
    static const char *attributes[]={"id","extends","class","title","sprite","damaged","idle","move","carry","run",
        "work","shadow","act","directions","variants","speed","opacity","ambient","effect","approach","action",
        "duration","cleared-frame","burnable","needs-dynamite","boat-only","resource","amount","produces","income",
        "income-interval","bonus","bonus-interval","menu","menu-lock",NULL};
    static const char *children[]={"cost","reward","build","repair",NULL};
    const char *id,*parent_id=xml_attr(node,"extends"),*kind=xml_attr(node,"class");
    if (!xml_only(doc,node,attributes,children) || !xml_require(doc,node,"id",&id)) return false;
    memset(out,0,sizeof *out);
    if (parent_id) {
        const Archetype *parent=lookup(parent_id,context);
        if (!parent) return xml_fail(doc,node,"extends unknown archetype \"%s\"",parent_id);
        *out=*parent;out->base=parent;
        if (kind && find_name(class_names,CLASS_COUNT,kind)!=(int)parent->kind)
            return xml_fail(doc,node,"cannot change class from %s",class_names[parent->kind]);
    } else {
        if (!kind) return xml_fail(doc,node,"needs a class or extends");
        int k=find_name(class_names,CLASS_COUNT,kind);
        if (k<0) return xml_fail(doc,node,"unknown class \"%s\"",kind);
        out->kind=(ArchetypeClass)k;class_defaults(out);
    }
    if (!(out->id=arena_strdup(a,id))) return false;
    static const char *const text_keys[]={"title","sprite","damaged","idle","move","carry","run","work","shadow","act","action","menu-lock"};
    const char **text_fields[]={&out->title,&out->sprite,&out->damaged,&out->idle,&out->move,&out->carry,&out->run,
        &out->work,&out->shadow,&out->act,&out->action,&out->menu_lock};
    for (size_t i=0;i<sizeof text_keys/sizeof *text_keys;i++) if (!copy_text(a,node,text_keys[i],text_fields[i])) return false;
    if (!xml_int(doc,node,"directions",out->directions,1,64,&out->directions) ||
        !xml_int(doc,node,"variants",out->variants,1,16,&out->variants) ||
        !xml_float(doc,node,"speed",out->speed,0,10000,&out->speed) ||
        !xml_float(doc,node,"opacity",out->opacity,0,1,&out->opacity) ||
        !xml_float(doc,node,"duration",out->duration,0,3600,&out->duration) ||
        !xml_int(doc,node,"cleared-frame",out->cleared_frame,-1,1000,&out->cleared_frame) ||
        !xml_bool(doc,node,"burnable",out->burnable,&out->burnable) ||
        !xml_bool(doc,node,"needs-dynamite",out->needs_dynamite,&out->needs_dynamite) ||
        !xml_bool(doc,node,"boat-only",out->boat_only,&out->boat_only) ||
        !xml_int(doc,node,"amount",out->amount,0,1000000,&out->amount) ||
        !xml_int(doc,node,"income",out->income,0,1000000,&out->income) ||
        !xml_float(doc,node,"income-interval",out->income_interval,0,3600,&out->income_interval) ||
        !xml_float(doc,node,"bonus-interval",out->bonus_interval,0,3600,&out->bonus_interval) ||
        !xml_int(doc,node,"menu",out->menu_slot,-1,MAX_BUILDABLE-1,&out->menu_slot) ||
        !resource_attribute(doc,node,"resource",&out->resource) ||
        !resource_attribute(doc,node,"produces",&out->produces)) return false;
    static const char *const ambients[]={"none","bird","cloud"},*const effects[]={"none","fog","fire","pollution"},
        *const approaches[]={"footprint","center","workpoint"};
    const char *text;
    if ((text=xml_attr(node,"ambient"))) {
        int v=find_name(ambients,3,text);if (v<0) return xml_fail(doc,node,"unknown ambient \"%s\"",text);
        out->ambient=(Ambient)v;
    }
    if ((text=xml_attr(node,"effect"))) {
        int v=find_name(effects,4,text);if (v<0) return xml_fail(doc,node,"unknown effect \"%s\"",text);
        out->effect=(VisualEffect)v;
    }
    if ((text=xml_attr(node,"approach"))) {
        int v=find_name(approaches,3,text);if (v<0) return xml_fail(doc,node,"unknown approach \"%s\"",text);
        out->approach=(Approach)v;
    }
    static const char *const stock_names[]={"cost","reward","build","repair"};
    Stock *stocks[]={&out->cost,&out->reward,&out->build_cost,&out->repair_cost};
    for (int i=0;i<4;i++) {
        int count=xml_count(node,stock_names[i]);
        if (count>1) return xml_fail(doc,node,"has more than one <%s>",stock_names[i]);
        if (count) {
            /* A child replaces the inherited amounts rather than merging with them. */
            memset(stocks[i],0,sizeof *stocks[i]);
            if (!content_stock(doc,xml_next(node,stock_names[i],NULL),stocks[i])) return false;
        }
    }
    *bonus_id=xml_attr(node,"bonus");
    if (out->kind==CLASS_PICKUP && out->resource==RESOURCE_NONE) return xml_fail(doc,node,"pickup needs a resource");
    if (out->kind==CLASS_PRODUCER && (out->produces==RESOURCE_NONE || !out->sprite || !out->damaged))
        return xml_fail(doc,node,"producer needs produces, sprite and damaged");
    return true;
}

static const Archetype *global_lookup(const char *id,void *context) { (void)context;return archetype_find(id); }

static bool parse_ruleset(const XmlDoc *doc,const XmlNode *node)
{
    static const char *attributes[]={"id","economy","auto-evacuation","vehicles","gates","wildlife","goals",NULL};
    const char *id;
    if (ruleset_count==MAX_RULESETS) return xml_fail(doc,node,"too many rulesets");
    Ruleset *r=&rulesets[ruleset_count];
    if (!xml_only(doc,node,attributes,NULL) || !xml_require(doc,node,"id",&id) ||
        !xml_bool(doc,node,"economy",false,&r->economy) || !xml_bool(doc,node,"auto-evacuation",false,&r->auto_evacuation) ||
        !xml_bool(doc,node,"vehicles",false,&r->vehicles) || !xml_bool(doc,node,"gates",false,&r->gates) ||
        !xml_bool(doc,node,"wildlife",false,&r->wildlife) || !xml_bool(doc,node,"goals",false,&r->goals)) return false;
    if (ruleset_find(id)) return xml_fail(doc,node,"duplicate ruleset \"%s\"",id);
    if (!(r->id=arena_strdup(&arena,id))) return false;
    ruleset_count++;return true;
}

static bool load_archetypes(const char *path)
{
    XmlDoc doc;
    if (!xml_load(&doc,path)) return false;
    static const char *children[]={"ruleset","archetype",NULL};
    const XmlNode *root=doc.root;
    bool ok=!strcmp(root->name,"content")?xml_only(&doc,root,NULL,children):xml_fail(&doc,root,"expected <content> root");
    const char *bonus_ids[MAX_ARCHETYPES]={0};
    XML_EACH(node,root,"ruleset") { if (!ok) break;ok=parse_ruleset(&doc,node); }
    XML_EACH(node,root,"archetype") {
        if (!ok) break;
        if (archetype_count==MAX_ARCHETYPES) { ok=xml_fail(&doc,node,"too many archetypes");break; }
        Archetype *a=arena_alloc(&arena,sizeof *a);
        if (!(ok=a && archetype_parse(&doc,node,&arena,global_lookup,NULL,a,&bonus_ids[archetype_count]))) break;
        if (archetype_find(a->id)) { ok=xml_fail(&doc,node,"duplicate archetype \"%s\"",a->id);break; }
        archetypes[archetype_count++]=a;
    }
    for (int i=0;i<archetype_count && ok;i++) if (bonus_ids[i]) {
        const Archetype *bonus=archetype_find(bonus_ids[i]);
        if (!bonus || bonus->kind!=CLASS_PICKUP) ok=error_set("%s: archetype %s: bonus \"%s\" is not a pickup",path,archetypes[i]->id,bonus_ids[i]);
        else archetypes[i]->bonus=bonus;
    }
    xml_free(&doc);return ok;
}

/* Reads rules, start stock (food/wood/money/fuel), goals (people/basins),
 * buildable and crew-at-hq attributes over DEFAULTS. Callers check which
 * attributes their element allows. */
bool mission_settings(const XmlDoc *doc,const XmlNode *node,const Mission *defaults,Mission *out)
{
    *out=*defaults;
    const char *rules=xml_attr(node,"rules");
    if (rules && !(out->rules=ruleset_find(rules))) return xml_fail(doc,node,"unknown ruleset \"%s\"",rules);
    for (int r=0;r<RESOURCE_COUNT;r++)
        if (!xml_int(doc,node,resource_names[r],out->start.amount[r],0,1000000,&out->start.amount[r])) return false;
    if (!xml_int(doc,node,"people",out->people_goal,0,1000000,&out->people_goal) ||
        !xml_int(doc,node,"basins",out->basin_goal,0,1000000,&out->basin_goal) ||
        !xml_bool(doc,node,"crew-at-hq",out->crew_at_hq,&out->crew_at_hq) ||
        !xml_bool(doc,node,"offroad",out->offroad,&out->offroad)) return false;
    const char *buildable=xml_attr(node,"buildable");
    if (buildable) {
        char list[512];
        if (!text_copy(list,sizeof list,buildable)) return xml_fail(doc,node,"buildable list too long");
        out->buildable_count=0;
        for (char *save=NULL,*id=strtok_r(list," ",&save);id;id=strtok_r(NULL," ",&save)) {
            const Archetype *a=archetype_find(id);
            if (!a || a->kind!=CLASS_PRODUCER || a->menu_slot<0) return xml_fail(doc,node,"\"%s\" is not a buildable producer",id);
            if (out->buildable_count==MAX_BUILDABLE) return xml_fail(doc,node,"too many buildable producers");
            out->buildable[out->buildable_count++]=a;
        }
    }
    if (!out->rules) return xml_fail(doc,node,"needs a ruleset");
    return true;
}

const Mission *campaign_defaults(void) { return &campaign_default; }

static bool load_campaign(const char *path)
{
    XmlDoc doc;
    if (!xml_load(&doc,path)) return false;
    static const char *root_attributes[]={"rules",NULL},*children[]={"mission",NULL};
    static const char *attributes[]={"number","rules","food","wood","money","fuel","people","basins","crew-at-hq","offroad","buildable",NULL};
    const XmlNode *root=doc.root;const char *rules;
    bool ok=!strcmp(root->name,"campaign")?xml_only(&doc,root,root_attributes,children):xml_fail(&doc,root,"expected <campaign> root");
    if (ok) ok=xml_require(&doc,root,"rules",&rules);
    if (ok && !(campaign_default.rules=ruleset_find(rules))) ok=xml_fail(&doc,root,"unknown ruleset \"%s\"",rules);
    campaign_default.buildable_count=content_menu(campaign_default.buildable,MAX_BUILDABLE);
    campaign_default.offroad=true;
    XML_EACH(node,root,"mission") {
        int number;
        if (!ok) break;
        if (!(ok=xml_only(&doc,node,attributes,NULL) && xml_int(&doc,node,"number",0,1,CAMPAIGN_MISSIONS,&number))) break;
        if (!number || campaign_defined[number]) { ok=xml_fail(&doc,node,"needs a unique number 1-%d",CAMPAIGN_MISSIONS);break; }
        if ((ok=mission_settings(&doc,node,&campaign_default,&campaign[number]))) campaign_defined[number]=true;
    }
    xml_free(&doc);return ok;
}

bool campaign_mission(int number,Mission *out)
{
    if (number<1 || number>CAMPAIGN_MISSIONS) return false;
    *out=campaign_defined[number]?campaign[number]:campaign_default;return true;
}

bool content_load(const char *folder)
{
    content_destroy();
    char path[PATH_CAPACITY];
    if (!text_copy(directory,sizeof directory,folder)) return error_set("Content path too long");
    bool ok=path_join(path,sizeof path,folder,"archetypes.xml") && load_archetypes(path) &&
        path_join(path,sizeof path,folder,"campaign.xml") && load_campaign(path);
    if (!ok) { char saved[sizeof last_error];memcpy(saved,last_error,sizeof saved);content_destroy();memcpy(last_error,saved,sizeof saved); }
    return ok;
}

void content_destroy(void)
{
    arena_free(&arena);archetype_count=ruleset_count=0;
    memset(campaign_defined,0,sizeof campaign_defined);memset(&campaign_default,0,sizeof campaign_default);
}
