/* Rescue Team 5 level binaries, version 301: little-endian scalars and
 * length-prefixed strings. Type 18 stores a traffic point list instead of
 * the usual object payload; type 6 appends the ambient object's initial
 * position. Read losslessly; nothing is rewritten. */
#include "content_internal.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(float)==4,"Original levels require 32-bit float storage");
enum { MAX_OBJECTS=4096,MAX_POINTS=4096 };

typedef struct { const unsigned char *bytes;size_t size,pos;bool valid;const char *path; } Reader;

static bool fail(Reader *r,const char *message)
{
    if (r->valid) error_set("%s: byte %zu: %s",r->path,r->pos,message);
    r->valid=false;return false;
}

static bool available(Reader *r,size_t n)
{
    if (!r->valid) return false;
    return n<=r->size-r->pos?true:fail(r,"truncated binary resource");
}

static uint32_t integer(Reader *r,int n)
{
    uint32_t value=0;
    if (!available(r,(size_t)n)) return 0;
    for (int i=0;i<n;i++) value|=(uint32_t)r->bytes[r->pos++]<<(8*i);
    return value;
}

static float scalar(Reader *r)
{
    uint32_t bits=integer(r,4);float value;memcpy(&value,&bits,sizeof value);
    if (r->valid && !isfinite(value)) fail(r,"non-finite object coordinate");
    return value;
}

static const char *string(Reader *r,Arena *arena,size_t capacity)
{
    size_t n=integer(r,2);
    if (!available(r,n)) return NULL;
    if (n>=capacity) { fail(r,"string exceeds supported capacity");return NULL; }
    if (memchr(r->bytes+r->pos,0,n)) { fail(r,"embedded NUL in string");return NULL; }
    char *text=arena_alloc(arena,n+1);
    if (!text) { fail(r,"out of memory");return NULL; }
    memcpy(text,r->bytes+r->pos,n);r->pos+=n;return text;
}

static bool decode(const unsigned char *bytes,size_t length,const char *path,RetailLevel *level)
{
    Reader r={bytes,length,0,true,path};
    memset(level,0,sizeof *level);
    level->version=integer(&r,4);
    if (!r.valid) return false;
    if (level->version!=301) return fail(&r,"unsupported original level version (expected 301)");
    if (!(level->background=string(&r,&level->arena,128))) return false;
    level->max_id=integer(&r,4);
    uint32_t count=integer(&r,4);
    if (!r.valid) return false;
    if (count>MAX_OBJECTS) return fail(&r,"too many objects");
    level->objects=arena_alloc(&level->arena,(count?count:1)*sizeof *level->objects);
    level->traffic=arena_alloc(&level->arena,MAX_POINTS*sizeof *level->traffic);
    if (!level->objects || !level->traffic) return fail(&r,"out of memory");
    for (uint32_t i=0;i<count;i++) {
        RetailObject *o=&level->objects[i];
        o->type=integer(&r,4);o->id=integer(&r,4);o->name=o->params=o->extra="";
        if (o->type==18) {
            uint32_t points=integer(&r,4);
            if (!r.valid) return false;
            if (points>MAX_POINTS-(uint32_t)level->traffic_count) return fail(&r,"too many traffic points");
            for (uint32_t j=0;j<points;j++) {
                Vec2 *p=&level->traffic[level->traffic_count++];
                p->x=scalar(&r);p->y=scalar(&r);
            }
            o->layer=integer(&r,4);o->scale_x=o->scale_y=1;
        } else {
            o->layer=integer(&r,4);
            if (!(o->name=string(&r,&level->arena,512)) || !(o->params=string(&r,&level->arena,4096)) ||
                !(o->extra=string(&r,&level->arena,4096))) return false;
            o->link_id=integer(&r,4);
            o->p.x=scalar(&r);o->p.y=scalar(&r);o->rotation=scalar(&r);
            o->scale_x=scalar(&r);o->scale_y=scalar(&r);
            if (o->type==6) { o->auxiliary.x=scalar(&r);o->auxiliary.y=scalar(&r); }
        }
        if (!r.valid) return false;
        level->object_count++;
    }
    if (r.pos!=r.size) return fail(&r,"unsupported trailing level data");
    return true;
}

bool retail_level_read(const char *path,RetailLevel *out)
{
    size_t length;unsigned char *data=(unsigned char *)file_read(path,64u<<20,&length);
    if (!data) { memset(out,0,sizeof *out);return false; }
    bool ok=decode(data,length,path,out);free(data);
    if (!ok) retail_level_free(out);
    return ok;
}

void retail_level_free(RetailLevel *level) { arena_free(&level->arena);memset(level,0,sizeof *level); }

bool retail_param(const char *params,const char *key,char *out,size_t size)
{
    if (!params || !key || !out || !size) return false;
    out[0]=0;size_t key_length=strlen(key);const char *p=params;
    while (*p) {
        while (*p && (isspace((unsigned char)*p) || *p==';')) p++;
        const char *end=p+strcspn(p,";\r\n"),*equals=memchr(p,'=',(size_t)(end-p));
        if (equals) {
            const char *key_end=equals;
            while (key_end>p && isspace((unsigned char)key_end[-1])) key_end--;
            if ((size_t)(key_end-p)==key_length && !memcmp(p,key,key_length)) {
                const char *value=equals+1;
                while (value<end && isspace((unsigned char)*value)) value++;
                while (end>value && isspace((unsigned char)end[-1])) end--;
                size_t n=(size_t)(end-value);
                if (n>=size) return false;
                memcpy(out,value,n);out[n]=0;return true;
            }
        }
        p=end;if (*p) p++;
    }
    return false;
}

/* Reads every original binary to its exact end and checks that truncated or
 * re-versioned data is rejected. Requires the retail level folder. */
bool retail_level_selftest(void)
{
    char folder[PATH_CAPACITY],path[PATH_CAPACITY];char **names;int count,tested=0;bool ok=true;
    if (!vfs_resolve("level/map.lvl",path,sizeof path)) return error_set("Retail level folder not found");
    path_directory(folder,sizeof folder,path);
    if (!dir_list(folder,".lvl",&names,&count)) return false;
    for (int i=0;i<count && ok;i++) {
        RetailLevel level;
        ok=path_join(path,sizeof path,folder,names[i]) && retail_level_read(path,&level);
        if (ok) { retail_level_free(&level);tested++; }
    }
    dir_list_free(names,count);
    size_t n=0;unsigned char *data=NULL;
    if (ok && path_join(path,sizeof path,folder,"level000.lvl")) data=(unsigned char *)file_read(path,64u<<20,&n);
    if (ok && !data) ok=false;
    if (ok) {
        RetailLevel level;
        ok=decode(data,n,path,&level) && level.object_count==194 && level.traffic_count==110 && !strcmp(level.background,"001");
        /* The two retail fires are object names, independent of tree00. */
        int burning=0,unlit=0;
        for (int i=0;ok && i<level.object_count;i++) {
            const RetailObject *o=&level.objects[i];
            if (o->type!=102 || strcmp(o->params,"tree00")) continue;
            if (o->id==1249 || o->id==1204) { ok=!strcmp(o->name,"fire");burning++; }
            else { ok=!o->name[0];unlit++; }
        }
        ok=ok && burning==2 && unlit==2;
        retail_level_free(&level);
        const size_t cuts[]={0,3,5,16,30,44,49,7913};
        for (size_t i=0;ok && i<sizeof cuts/sizeof *cuts;i++)
            if (cuts[i]<n && decode(data,cuts[i],path,&level)) ok=false;else retail_level_free(&level);
        if (ok && n && decode(data,n-1,path,&level)) ok=false;else retail_level_free(&level);
        unsigned char old=data[0];data[0]=0;
        if (ok && decode(data,n,path,&level)) ok=false;else retail_level_free(&level);
        data[0]=old;
    }
    free(data);
    char value[32];
    if (ok) ok=retail_param("anim=water0;random","anim",value,sizeof value) && !strcmp(value,"water0") &&
        !retail_param("anim=water0","missing",value,sizeof value) && !retail_param("anim=water0","anim",value,2);
    if (!ok || !tested) {
        char reason[sizeof last_error];memcpy(reason,last_error,sizeof reason);
        return error_set("Retail level binary self-test failed: %s",reason);
    }
    error_clear();
    printf("Validated %d original binary levels; malformed/truncated input rejected\n",tested);
    return true;
}
