#include "volcano.h"
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(sizeof(float)==4,"Original levels require 32-bit float storage");

/* Rescue Team 5 version 301: little endian scalars, length-prefixed strings.
   Type 18 stores a traffic point list instead of the usual object payload;
   type 6 appends the ambient object's initial position. No resource is rewritten. */
typedef struct {
    const unsigned char *bytes;
    size_t size,pos;
    bool valid;
    char *error;
    size_t error_size;
} Reader;

static bool fail(Reader *r,const char *message) {
    if(r->valid && r->error_size)
        snprintf(r->error,r->error_size,"Level byte %zu: %s",r->pos,message);
    r->valid=false;
    return false;
}
static bool available(Reader *r,size_t n) {
    if(!r->valid) return false;
    if(n>r->size-r->pos) return fail(r,"truncated binary resource");
    return true;
}
static uint32_t integer(Reader *r,int n) {
    uint32_t value=0;
    if(!available(r,(size_t)n)) return 0;
    for(int i=0;i<n;i++) value|=(uint32_t)r->bytes[r->pos++]<<(8*i);
    return value;
}
static float scalar(Reader *r) {
    uint32_t bits=integer(r,4);
    float value;
    memcpy(&value,&bits,sizeof value);
    if(r->valid && !isfinite(value)) fail(r,"non-finite object coordinate");
    return value;
}
static bool string(Reader *r,char *out,size_t capacity) {
    size_t n=integer(r,2);
    if(!available(r,n)) return false;
    if(n>=capacity) return fail(r,"string exceeds supported capacity");
    if(memchr(r->bytes+r->pos,0,n)) return fail(r,"embedded NUL in string");
    memcpy(out,r->bytes+r->pos,n);out[n]=0;r->pos+=n;
    return true;
}
static bool decode(const unsigned char *bytes,size_t length,Level *level,char *error,size_t error_size) {
    Reader r={bytes,length,0,true,error,error_size};
    if(error_size) error[0]=0;
    memset(level,0,sizeof *level);
    level->version=integer(&r,4);
    if(!r.valid) return false;
    if(level->version!=301) return fail(&r,"unsupported original level version (expected 301)");
    if(!string(&r,level->background,sizeof level->background)) return false;
    level->max_id=integer(&r,4);
    uint32_t count=integer(&r,4);
    if(!r.valid) return false;
    if(count>MAX_LEVEL_OBJECTS) return fail(&r,"too many objects");
    for(uint32_t i=0;i<count;i++) {
        LevelObject *o=&level->objects[i];
        o->type=integer(&r,4);o->id=integer(&r,4);
        if(o->type==18) {
            uint32_t points=integer(&r,4);
            if(!r.valid) return false;
            if(points>MAX_TRAFFIC_POINTS-(uint32_t)level->traffic_count)
                return fail(&r,"too many traffic points");
            for(uint32_t j=0;j<points;j++) {
                Vec2 *p=&level->traffic[level->traffic_count++];
                p->x=scalar(&r);p->y=scalar(&r);
            }
            o->layer=integer(&r,4);
            o->scale_x=o->scale_y=1;
        } else {
            o->layer=integer(&r,4);
            if(!string(&r,o->name,sizeof o->name) ||
               !string(&r,o->params,sizeof o->params) ||
               !string(&r,o->extra,sizeof o->extra)) return false;
            o->link_id=integer(&r,4);
            o->x=scalar(&r);o->y=scalar(&r);o->rotation=scalar(&r);
            o->scale_x=scalar(&r);o->scale_y=scalar(&r);
            if(o->type==6) {o->auxiliary.x=scalar(&r);o->auxiliary.y=scalar(&r);}
        }
        if(!r.valid) return false;
        level->object_count++;
    }
    level->trailing_bytes=r.size-r.pos;
    if(level->trailing_bytes) return fail(&r,"unsupported trailing level data");
    return true;
}
static unsigned char *read_binary(const char *path,size_t *length,char *error,size_t size) {
    FILE *f=fopen(path,"rb");
    if(!f) {if(size)snprintf(error,size,"Cannot open %s: %s",path,strerror(errno));return NULL;}
    if(fseek(f,0,SEEK_END)!=0) {if(size)snprintf(error,size,"Cannot seek %s",path);fclose(f);return NULL;}
    long n=ftell(f);
    if(n<0 || n>64*1024*1024 || fseek(f,0,SEEK_SET)!=0) {
        if(size)snprintf(error,size,"Invalid or oversized level: %s",path);
        fclose(f);return NULL;
    }
    unsigned char *data=malloc(n?(size_t)n:1);
    if(!data) {if(size)snprintf(error,size,"Cannot allocate level buffer");fclose(f);return NULL;}
    if(fread(data,1,(size_t)n,f)!=(size_t)n || ferror(f)) {
        if(size)snprintf(error,size,"Cannot read %s",path);
        free(data);fclose(f);return NULL;
    }
    fclose(f);*length=(size_t)n;return data;
}
bool level_load(const char *path,Level *level,char *error,size_t size) {
    if(!path || !level || (!error && size)) return false;
    size_t length=0;
    unsigned char *data=read_binary(path,&length,error,size);
    if(!data) return false;
    bool result=decode(data,length,level,error,size);
    free(data);return result;
}
bool level_param(const char *params,const char *key,char *out,size_t size) {
    if(!params || !key || !out || !size) return false;
    out[0]=0;
    size_t key_length=strlen(key);
    const char *p=params;
    while(*p) {
        while(*p && (isspace((unsigned char)*p) || *p==';'))p++;
        const char *end=p+strcspn(p,";\r\n");
        const char *equals=memchr(p,'=',(size_t)(end-p));
        if(equals) {
            const char *key_end=equals;
            while(key_end>p && isspace((unsigned char)key_end[-1]))key_end--;
            if((size_t)(key_end-p)==key_length && !memcmp(p,key,key_length)) {
                const char *value=equals+1;
                while(value<end && isspace((unsigned char)*value))value++;
                while(end>value && isspace((unsigned char)end[-1]))end--;
                size_t n=(size_t)(end-value);
                if(n>=size)return false;
                memcpy(out,value,n);out[n]=0;return true;
            }
        }
        p=end;
        if(*p)p++;
    }
    return false;
}
bool level_selftest(const char *root) {
    char directory[PATH_CAPACITY],path[PATH_CAPACITY],error[1024]="level path exceeds capacity";
    if(snprintf(directory,sizeof directory,"%s/level",root)>=(int)sizeof directory)return false;
    DIR *dir=opendir(directory);
    if(!dir) {fprintf(stderr,"Level test: cannot open %s\n",directory);return false;}
    Level *level=malloc(sizeof *level);
    if(!level) {closedir(dir);return false;}
    int tested=0;bool ok=true;struct dirent *entry;
    while((entry=readdir(dir))) {
        size_t n=strlen(entry->d_name);
        if(n<4 || strcmp(entry->d_name+n-4,".lvl"))continue;
        if(snprintf(path,sizeof path,"%s/%s",directory,entry->d_name)>=(int)sizeof path ||
           !level_load(path,level,error,sizeof error)) {
            fprintf(stderr,"Level test: %s: %s\n",entry->d_name,error);ok=false;break;
        }
        tested++;
    }
    closedir(dir);
    /* Exercise bounds failures and unknown versions without touching originals. */
    if(ok) {
        if(snprintf(path,sizeof path,"%s/level000.lvl",directory)>=(int)sizeof path)ok=false;
        size_t n=0;unsigned char *data=ok?read_binary(path,&n,error,sizeof error):NULL;
        if(!data)ok=false;
        else {
            ok=decode(data,n,level,error,sizeof error) && level->object_count==194 &&
               level->traffic_count==110 && !strcmp(level->background,"001");
            const size_t cuts[]={0,3,5,16,30,44,49,7913};
            for(size_t i=0;ok && i<sizeof cuts/sizeof cuts[0];i++)
                if(cuts[i]<n && decode(data,cuts[i],level,error,sizeof error))ok=false;
            if(ok && n && decode(data,n-1,level,error,sizeof error))ok=false;
            unsigned char old=data[0];data[0]=0;
            if(ok && decode(data,n,level,error,sizeof error))ok=false;
            data[0]=old;free(data);
        }
    }
    char value[32];
    if(ok)ok=level_param("anim=water0;random","anim",value,sizeof value) && !strcmp(value,"water0") &&
             !level_param("anim=water0","missing",value,sizeof value) &&
             !level_param("anim=water0","anim",value,2);
    free(level);
    if(!ok || !tested) {fprintf(stderr,"Level binary self-test failed\n");return false;}
    printf("Validated %d original binary levels; malformed/truncated input rejected\n",tested);
    return true;
}
