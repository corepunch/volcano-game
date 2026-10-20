/* Retail campaign order. File numbering is not play order: type-135 map
 * nodes name a scene background and its prerequisite node. Following the
 * chain from the empty prerequisite recovers all fifty missions. */
#include "content_internal.h"
#include <stdio.h>
#include <string.h>

enum { MISSIONS=50 };
static bool loaded;
static int order[MISSIONS];

static bool retail_path(char *out,size_t size,const char *name)
{
    char relative[64];snprintf(relative,sizeof relative,"level/%s",name);
    if (!vfs_resolve(relative,out,size)) return error_set("Missing original level file %s",relative);
    return true;
}

bool campaign_load(void)
{
    if (loaded) return true;
    char path[PATH_CAPACITY],names[MISSIONS][128],parents[MISSIONS][128],scenes[MISSIONS][128];
    int count=0,next_order[MISSIONS];bool seen[MISSIONS]={false};RetailLevel level;
    if (!retail_path(path,sizeof path,"map.lvl") || !retail_level_read(path,&level)) return false;
    for (int i=0;i<level.object_count && count<=MISSIONS;i++) if (level.objects[i].type==135) {
        if (count<MISSIONS) {
            snprintf(names[count],128,"%.127s",level.objects[i].name);
            snprintf(parents[count],128,"%.127s",level.objects[i].extra);
        }
        count++;
    }
    retail_level_free(&level);
    if (count!=MISSIONS) return error_set("Original campaign map has %d nodes, expected %d",count,MISSIONS);
    for (int i=0;i<MISSIONS;i++) {
        char name[32];snprintf(name,sizeof name,"level%03d.lvl",i);
        if (!retail_path(path,sizeof path,name) || !retail_level_read(path,&level)) return false;
        snprintf(scenes[i],128,"%.127s",level.background);retail_level_free(&level);
    }
    const char *previous="";
    for (int n=0;n<MISSIONS;n++) {
        int node=-1,index=-1;
        for (int i=0;i<MISSIONS;i++) if (!strcmp(parents[i],previous)) {
            if (node>=0) return error_set("Campaign map branches after %s",previous);
            node=i;
        }
        if (node<0 || seen[node]) return error_set("Campaign map chain is broken after \"%s\"",previous);
        seen[node]=true;
        for (int i=0;i<MISSIONS;i++) if (!strcmp(names[node],scenes[i])) {
            if (index>=0) return error_set("Campaign scene %s is ambiguous",names[node]);
            index=i;
        }
        if (index<0) return error_set("Campaign scene %s has no level file",names[node]);
        next_order[n]=index;previous=names[node];
    }
    memcpy(order,next_order,sizeof order);loaded=true;return true;
}

int campaign_index(int number) { return loaded && number>=1 && number<=MISSIONS?order[number-1]:-1; }

int campaign_number(int index)
{
    for (int i=0;loaded && i<MISSIONS;i++) if (order[i]==index) return i+1;
    return 0;
}
