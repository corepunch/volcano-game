#include "core.h"
#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

char last_error[1024];

bool error_set(const char *format,...)
{
    va_list args;va_start(args,format);vsnprintf(last_error,sizeof last_error,format,args);va_end(args);
    return false;
}

void error_clear(void) { last_error[0]=0; }

float vec2_distance(Vec2 a,Vec2 b) { return hypotf(a.x-b.x,a.y-b.y); }

float hash_random(uint32_t seed)
{
    seed^=seed>>16;seed*=0x7feb352du;seed^=seed>>15;seed*=0x846ca68bu;seed^=seed>>16;
    return (seed>>8)*(1.f/16777216.f);
}

uint32_t utf8_next(const char **text)
{
    const unsigned char *s=(const unsigned char *)*text;uint32_t c=*s++;
    if (c>=0xF0 && s[0] && s[1] && s[2]) { c=(c&7)<<18|(s[0]&63)<<12|(s[1]&63)<<6|(s[2]&63);s+=3; }
    else if (c>=0xE0 && s[0] && s[1]) { c=(c&15)<<12|(s[0]&63)<<6|(s[1]&63);s+=2; }
    else if (c>=0xC0 && s[0]) { c=(c&31)<<6|(s[0]&63);s++; }
    *text=(const char *)s;return c;
}

bool text_copy(char *out,size_t size,const char *text)
{
    size_t n=strlen(text);
    if (n>=size) return false;
    memcpy(out,text,n+1);return true;
}

bool path_join(char *out,size_t size,const char *directory,const char *relative)
{
    if (relative[0]=='/' || !directory[0]) return text_copy(out,size,relative);
    size_t n=strlen(directory);bool slash=directory[n-1]=='/';
    int written=snprintf(out,size,"%s%s%s",directory,slash?"":"/",relative);
    return written>=0 && (size_t)written<size;
}

void path_directory(char *out,size_t size,const char *path)
{
    const char *slash=strrchr(path,'/');size_t n=slash?(size_t)(slash-path+1):0;
    if (n>=size) n=0;
    memcpy(out,path,n);out[n]=0;
}

bool file_exists(const char *path)
{
    struct stat info;return !stat(path,&info) && S_ISREG(info.st_mode);
}

static bool directory_exists(const char *path)
{
    struct stat info;return !stat(path,&info) && S_ISDIR(info.st_mode);
}

char *file_read(const char *path,size_t limit,size_t *length)
{
    FILE *file=fopen(path,"rb");
    if (!file) { error_set("Cannot open %s: %s",path,strerror(errno));return NULL; }
    char *data=NULL;long size=-1;
    if (!fseek(file,0,SEEK_END)) size=ftell(file);
    if (size<0 || (size_t)size>limit || fseek(file,0,SEEK_SET)) {
        error_set("Invalid or oversized file: %s",path);
    } else if (!(data=malloc((size_t)size+1))) {
        error_set("Cannot allocate %ld bytes for %s",size,path);
    } else if (fread(data,1,(size_t)size,file)!=(size_t)size) {
        error_set("Cannot read %s",path);free(data);data=NULL;
    } else {
        data[size]=0;if (length) *length=(size_t)size;
    }
    fclose(file);return data;
}

/* Arena */

struct ArenaBlock { ArenaBlock *next;size_t used,capacity;_Alignas(16) unsigned char bytes[]; };

void *arena_alloc(Arena *arena,size_t size)
{
    size=(size+15)&~(size_t)15;
    ArenaBlock *block=arena->blocks;
    if (!block || block->capacity-block->used<size) {
        size_t capacity=size>16384?size:16384;
        block=malloc(sizeof *block+capacity);
        if (!block) { error_set("Out of memory");return NULL; }
        block->next=arena->blocks;block->used=0;block->capacity=capacity;arena->blocks=block;
    }
    void *result=block->bytes+block->used;block->used+=size;
    memset(result,0,size);return result;
}

char *arena_strdup(Arena *arena,const char *text)
{
    size_t n=strlen(text);char *copy=arena_alloc(arena,n+1);
    if (copy) memcpy(copy,text,n+1);
    return copy;
}

void arena_free(Arena *arena)
{
    while (arena->blocks) { ArenaBlock *next=arena->blocks->next;free(arena->blocks);arena->blocks=next; }
}

/* Virtual file system */

static char mounts[VFS_MAX_MOUNTS][PATH_CAPACITY];
static int mount_count;

void vfs_reset(void) { mount_count=0; }

bool vfs_mount(const char *directory)
{
    if (mount_count==VFS_MAX_MOUNTS) return error_set("Too many asset mounts (maximum %d)",VFS_MAX_MOUNTS);
    if (!directory_exists(directory)) return error_set("Asset folder not found: %s",directory);
    if (!text_copy(mounts[mount_count],PATH_CAPACITY,directory)) return error_set("Asset path too long: %s",directory);
    mount_count++;return true;
}

int vfs_mount_count(void) { return mount_count; }
const char *vfs_mount_path(int index) { return index>=0 && index<mount_count?mounts[index]:NULL; }

bool vfs_resolve(const char *relative,char *out,size_t size)
{
    for (int i=mount_count-1;i>=0;i--)
        if (path_join(out,size,mounts[i],relative) && file_exists(out)) return true;
    return false;
}

static int compare_names(const void *a,const void *b) { return strcmp(*(char *const *)a,*(char *const *)b); }

bool dir_list(const char *directory,const char *suffix,char ***out,int *out_count)
{
    *out=NULL;*out_count=0;
    DIR *dir=opendir(directory);if (!dir) return true;
    size_t suffix_length=strlen(suffix);char **names=NULL;int count=0;bool ok=true;struct dirent *entry;
    while ((entry=readdir(dir))) {
        size_t n=strlen(entry->d_name);
        if (n<=suffix_length || strcmp(entry->d_name+n-suffix_length,suffix) || entry->d_name[0]=='.') continue;
        char **grown=realloc(names,(size_t)(count+1)*sizeof *names);
        if (!grown) { ok=false;break; }
        names=grown;
        if (!(names[count]=strdup(entry->d_name))) { ok=false;break; }
        count++;
    }
    closedir(dir);
    if (!ok) { dir_list_free(names,count);return error_set("Out of memory listing %s",directory); }
    qsort(names,(size_t)count,sizeof *names,compare_names);
    *out=names;*out_count=count;return true;
}

void dir_list_free(char **names,int count)
{
    for (int i=0;i<count;i++) free(names[i]);
    free(names);
}

bool vfs_each(const char *directory,const char *suffix,VfsVisit visit,void *user)
{
    bool ok=true;
    for (int m=0;m<mount_count && ok;m++) {
        char folder[PATH_CAPACITY];char **names;int count;
        if (!path_join(folder,sizeof folder,mounts[m],directory)) return error_set("Asset path too long");
        if (!dir_list(folder,suffix,&names,&count)) return false;
        for (int i=0;i<count && ok;i++) {
            char path[PATH_CAPACITY],relative[PATH_CAPACITY];bool shadowed=false;
            if (!path_join(relative,sizeof relative,directory,names[i])) { ok=error_set("Asset path too long");break; }
            for (int upper=m+1;upper<mount_count && !shadowed;upper++)
                shadowed=path_join(path,sizeof path,mounts[upper],relative) && file_exists(path);
            if (!shadowed && path_join(path,sizeof path,folder,names[i])) ok=visit(path,names[i],m,user);
        }
        dir_list_free(names,count);
    }
    return ok;
}
