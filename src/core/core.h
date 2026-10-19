#ifndef VOLCANO_CORE_H
#define VOLCANO_CORE_H
/* Foundation shared by every layer. No SDL: simulation, content and tools
 * build against this header alone. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GAME_WIDTH 1366
#define GAME_HEIGHT 768
#define PATH_CAPACITY 2048
#define PI 3.14159265358979323846

typedef struct { float x,y; } Vec2;
typedef struct { int x,y,w,h; } RectI;

static inline Vec2 vec2(float x,float y) { return (Vec2){x,y}; }
float vec2_distance(Vec2 a,Vec2 b);
/* Deterministic hash noise in [0,1). Used for cosmetic variation only. */
float hash_random(uint32_t seed);

/* One error slot per process, like SDL_GetError(). Functions that fail return
 * false (or NULL) after describing the failure here; callers never print. */
extern char last_error[1024];
bool error_set(const char *format,...)
#if defined(__GNUC__)
    __attribute__((format(printf,1,2)))
#endif
    ;
void error_clear(void);

/* Decodes one UTF-8 code point and advances TEXT (lenient: never reads past NUL). */
uint32_t utf8_next(const char **text);
/* Bounded string copy and path joining; false when the result would truncate. */
bool text_copy(char *out,size_t size,const char *text);
bool path_join(char *out,size_t size,const char *directory,const char *relative);
/* Directory part of PATH including the trailing separator ("" for none). */
void path_directory(char *out,size_t size,const char *path);
bool file_exists(const char *path);
/* Whole-file read with a size cap; caller frees. Appends a NUL terminator. */
char *file_read(const char *path,size_t limit,size_t *length);
/* Sorted names in DIRECTORY ending in SUFFIX (empty when the folder is absent).
 * Returns false only when out of memory. Free with dir_list_free(). */
bool dir_list(const char *directory,const char *suffix,char ***names,int *count);
void dir_list_free(char **names,int count);

/* Arena: many small allocations freed together. */
typedef struct ArenaBlock ArenaBlock;
typedef struct { ArenaBlock *blocks; } Arena;
void *arena_alloc(Arena *arena,size_t size);
char *arena_strdup(Arena *arena,const char *text);
void arena_free(Arena *arena);

/* Layered asset mounts. Later mounts take precedence for files that exist in
 * several layers; directory listings are the union, shadowed by file name. */
#define VFS_MAX_MOUNTS 16
void vfs_reset(void);
bool vfs_mount(const char *directory);
int vfs_mount_count(void);
const char *vfs_mount_path(int index);
/* Topmost mount containing RELATIVE; false when absent. */
bool vfs_resolve(const char *relative,char *out,size_t size);
typedef bool (*VfsVisit)(const char *path,const char *name,int mount,void *user);
/* Visits files in DIRECTORY ending in SUFFIX: bottom mount first, names sorted,
 * each name once (from its topmost mount). Stops when VISIT returns false. */
bool vfs_each(const char *directory,const char *suffix,VfsVisit visit,void *user);
#endif
