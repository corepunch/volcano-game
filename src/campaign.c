#include "volcano.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char loaded_root[PATH_CAPACITY];
static int order[50];

bool campaign_load(const char *root)
{
    if (*loaded_root && !strcmp(root,loaded_root)) return true;
    Level *level=malloc(sizeof *level);
    if (!level) return false;
    char path[PATH_CAPACITY],names[50][128],parents[50][128],scenes[50][128];
    int count=0,next_order[50];bool valid=false,seen[50]={false};
    snprintf(path,sizeof path,"%s/level/map.lvl",root);
    if (!level_load(path,level,last_error,sizeof last_error)) goto done;
    for (int i=0;i<level->object_count;i++) if (level->objects[i].type==135) {
        if (count==50) goto done;
        snprintf(names[count],128,"%.127s",level->objects[i].name);
        snprintf(parents[count++],128,"%.127s",level->objects[i].extra);
    }
    if (count!=50) goto done;
    for (int i=0;i<50;i++) {
        snprintf(path,sizeof path,"%s/level/level%03d.lvl",root,i);
        if (!level_load(path,level,last_error,sizeof last_error)) goto done;
        snprintf(scenes[i],128,"%s",level->background);
    }
    const char *previous="";
    for (int n=0;n<50;n++) {
        int node=-1,index=-1;
        for (int i=0;i<50;i++) if (!strcmp(parents[i],previous)) {
            if (node>=0) goto done;
            node=i;
        }
        if (node<0 || seen[node]) goto done;
        seen[node]=true;
        for (int i=0;i<50;i++) if (!strcmp(names[node],scenes[i])) {
            if (index>=0) goto done;
            index=i;
        }
        if (index<0) goto done;
        next_order[n]=index;previous=names[node];
    }
    memcpy(order,next_order,sizeof order);
    snprintf(loaded_root,sizeof loaded_root,"%s",root);valid=true;
done:
    free(level);
    if (!valid && !last_error[0]) snprintf(last_error,sizeof last_error,"Invalid original campaign map");
    return valid;
}
int campaign_index(int number) { return *loaded_root && number>=1 && number<=50?order[number-1]:-1; }
int campaign_number(int index)
{
    if (*loaded_root) for (int i=0;i<50;i++) if (order[i]==index) return i+1;
    return 0;
}
