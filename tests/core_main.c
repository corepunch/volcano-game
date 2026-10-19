/* Core suite: links only the SDL-free layers (core, assets, content, sim).
 *   build/test_core ASSET_FOLDER
 * Covers the XML reader, layered mounts, content inheritance, every mission
 * file, and headless simulation: workers stop at the edge of a target's
 * footprint and face it while working. */
#include "../src/content/content_internal.h"
#include "../src/sim/world.h"
#include "../src/assets/assets.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures;
#define CHECK(condition,...) do { if (!(condition)) { failures++;fprintf(stderr,"FAIL %s:%d: ",__FILE__,__LINE__);fprintf(stderr,__VA_ARGS__);fprintf(stderr," [%s]\n",last_error); } } while (0)

static bool parses(const char *text)
{
    XmlDoc doc;bool ok=xml_parse(&doc,text,strlen(text),"test.xml");
    if (ok) xml_free(&doc);
    return ok;
}

static void xml_tests(void)
{
    CHECK(parses("<?xml version='1.0'?><!-- note --><a x='1' y=\"&lt;&#65;&#x42;\"><b/><b k='v'></b></a>"),"well-formed document");
    CHECK(parses("<a><![CDATA[<raw>]]></a>"),"CDATA");
    static const char *rejected[]={
        "<!DOCTYPE a [<!ENTITY x 'y'>]><a/>","<a x='1' x='2'/>","<a><b></a>","<a>text<b/></a>","<a>&custom;</a>",
        "<a/><b/>","<a x=1/>","<a","<a><!-- -- --></a>","<a>&#0;</a>",""
    };
    for (size_t i=0;i<sizeof rejected/sizeof *rejected;i++) CHECK(!parses(rejected[i]),"rejects %s",rejected[i]);
    XmlDoc doc;const char *text="<m><traffic>1,2 3.5,-4\n 5,6</traffic><o n='3' f='0.5,0.25'/></m>";
    CHECK(xml_parse(&doc,text,strlen(text),"points.xml"),"points document");
    if (doc.root) {
        Arena arena={0};Vec2 *points;int count=0;float pair[2];int n;
        CHECK(xml_points(&doc,xml_next(doc.root,"traffic",NULL),&arena,&points,&count) && count==3 &&
              points[1].x==3.5f && points[1].y==-4,"traffic points");
        const XmlNode *o=xml_next(doc.root,"o",NULL);
        CHECK(xml_floats(&doc,o,"f",pair,2,true) && pair[1]==.25f,"comma lists");
        CHECK(xml_int(&doc,o,"n",0,0,5,&n) && n==3 && !xml_int(&doc,o,"n",0,0,2,&n),"bounded integers");
        arena_free(&arena);xml_free(&doc);
    }
    error_clear();
}

static void write_file(const char *path,const char *text)
{
    FILE *f=fopen(path,"w");if (f) { fputs(text,f);fclose(f); }
}

static bool collect(const char *path,const char *name,int mount,void *user)
{
    (void)path;char *list=user;
    char entry[64];snprintf(entry,sizeof entry,"%s@%d ",name,mount);strcat(list,entry);return true;
}

/* Native sheets: pivot and body are normalized to the rectangle, so the same
 * numbers hold at any art resolution; world size is rect x scale. */
static void sheet_tests(void)
{
    char root[]="/tmp/volcano-sheet-XXXXXX",image[96],sheet[96],cwd[1024],source[1100];
    if (!mkdtemp(root) || !getcwd(cwd,sizeof cwd)) { CHECK(false,"sheet fixture");return; }
    snprintf(source,sizeof source,"%s/missions/twisted-tracks/debris.png",cwd);
    snprintf(image,sizeof image,"%s/art.png",root);snprintf(sheet,sizeof sheet,"%s/art.sprites.xml",root);
    CHECK(!symlink(source,image),"sheet image link");
    const char *files[]={sheet};
    write_file(sheet,"<sprites image=\"art.png\" scale=\"0.5\">"
        "<sprite name=\"t/a\" rect=\"0,0,200,100\" pivot=\"0.25,0.8\" body=\"0.1,0.2,0.5,0.5\"/></sprites>");
    const Sprite *s;
    CHECK(assets_load_level(NULL,0,files,1) && (s=sprite_find("t/a")) && s->frame_count==1,"normalized sheet loads");
    if ((s=sprite_find("t/a"))) {
        SpriteFrame f=s->frames[0],b=sprite_body_frame(f);
        CHECK(f.w==100 && f.h==50 && f.dx==25 && fabsf(f.dy-10)<1e-4f,"pivot maps to world units: %g,%g %gx%g",f.dx,f.dy,f.w,f.h);
        CHECK(b.w==50 && b.h==25 && b.dx==15 && fabsf(b.dy+5)<1e-4f,"body maps to world units: %g,%g %gx%g",b.dx,b.dy,b.w,b.h);
    }
    write_file(sheet,"<sprites image=\"art.png\"><sprite name=\"t/a\" rect=\"0,0,20,10\" anchor=\"1,1\"/></sprites>");
    CHECK(!assets_load_level(NULL,0,files,1),"absolute anchors are rejected");
    error_clear();assets_clear_level();
    unlink(sheet);unlink(image);rmdir(root);
}

static void vfs_tests(const char *assets)
{
    char root[]="/tmp/volcano-vfs-XXXXXX",a[64],b[64],path[128];
    if (!mkdtemp(root)) { CHECK(false,"temporary folder");return; }
    snprintf(a,sizeof a,"%s/a",root);snprintf(b,sizeof b,"%s/b",root);
    for (int i=0;i<2;i++) {
        char dir[96];snprintf(dir,sizeof dir,"%s/sprites",i?b:a);
        mkdir(i?b:a,0700);mkdir(dir,0700);
    }
    snprintf(path,sizeof path,"%s/sprites/one.txt",a);write_file(path,"a");
    snprintf(path,sizeof path,"%s/sprites/two.txt",a);write_file(path,"a");
    snprintf(path,sizeof path,"%s/sprites/two.txt",b);write_file(path,"b");
    vfs_reset();
    CHECK(vfs_mount(a) && vfs_mount(b),"mounts");
    char found[PATH_CAPACITY],list[256]="";
    CHECK(vfs_resolve("sprites/two.txt",found,sizeof found) && strstr(found,"/b/"),"upper mount wins");
    CHECK(vfs_resolve("sprites/one.txt",found,sizeof found) && strstr(found,"/a/"),"lower mount still visible");
    CHECK(vfs_each("sprites",".txt",collect,list) && !strcmp(list,"one.txt@0 two.txt@1 "),"listing is shadowed by name: %s",list);
    CHECK(!vfs_mount("/no/such/folder"),"missing folder rejected");
    for (const char *f[]={"a/sprites/one.txt","a/sprites/two.txt","b/sprites/two.txt","a/sprites","b/sprites","a","b"},**p=f;p<f+7;p++) {
        snprintf(path,sizeof path,"%s/%s",root,*p);remove(path);
    }
    rmdir(root);
    vfs_reset();vfs_mount(assets);error_clear();
}

static void content_tests(void)
{
    const Archetype *stone=archetype_find("stone"),*boulder=archetype_find("boulder"),*dog=archetype_find("dog");
    CHECK(stone && boulder && boulder->base==stone && boulder->cost.money==100 && boulder->cost.food==40 &&
          !strcmp(boulder->action,"pick") && !strcmp(boulder->sprite,"bar_stone_big00"),"extends inherits and overrides");
    CHECK(dog && dog->cost.food==10 && !strcmp(dog->title,"Rescue a dog") && dog->variants==3,"animal inheritance");
    CHECK(archetype_find("cafe")->bonus==archetype_find("food-small"),"bonus pickups resolve");
    const Archetype *menu[4];
    CHECK(content_menu(menu,4)==4 && !strcmp(menu[1]->id,"cafe") && menu[3]->build_cost.wood==80,"construction menu order");
    Mission m;
    CHECK(campaign_mission(6,&m) && m.rules->economy && m.basin_goal==2 && m.start.fuel==220 &&
          !mission_allows(&m,archetype_find("cafe")) && mission_allows(&m,archetype_find("house")),"campaign mission 6");
    CHECK(campaign_mission(20,&m) && !m.rules->economy && !m.rules->goals,"later campaign uses the prototype rules");
    CHECK(archetype_find("tree")->approach==APPROACH_FOOTPRINT && archetype_find("food-small")->approach==APPROACH_CENTER,
          "obstacles are worked from their footprint edge, pickups at their centre");
}

static Game *start(const char *path)
{
    LevelSpec spec;
    if (!mission_load(path,&spec)) return NULL;
    if (!assets_load_level(spec.packs,spec.pack_count,spec.sprite_files,spec.sprite_file_count)) { level_spec_free(&spec);return NULL; }
    return game_world_create(&spec);
}

static void mission_tests(void)
{
    static const char *missions[]={"first-rescue","emerald-falls","forest-mayday","ancient-echoes","twisted-tracks","library-rescue"};
    for (size_t i=0;i<sizeof missions/sizeof *missions;i++) {
        char path[256];snprintf(path,sizeof path,"missions/%s/mission.xml",missions[i]);
        Game *g=start(path);
        CHECK(g && g->worker_count>0 && g->traffic_count>0 && g->goal>0,"mission %s loads",missions[i]);
        game_world_free(g);
    }
    Game *g=start("missions/library-rescue/mission.xml");
    if (g) {
        const Entity *kitchen=&g->objects[0];
        for (int i=0;i<g->object_count;i++) if (g->objects[i].id==30) kitchen=&g->objects[i];
        CHECK(!strcmp(kitchen->type->id,"field-kitchen") && archetype_is(kitchen->type,archetype_find("cafe")) &&
              kitchen->type->repair_cost.wood==20 && !strcmp(game_entity_title(g,kitchen),"Field kitchen"),
              "mission archetype extends a producer and keeps its rules");
        game_world_free(g);
    }
    static const char *broken[]={
        "<mission format='volcano-mission-1' retail-background='103'><settings rules='rescue'/><object id='1' type='nope'/></mission>",
        "<mission format='volcano-mission-1' retail-background='103'><settings rules='rescue'/><object id='1' type='hq'/><object id='1' type='hq'/></mission>",
        "<mission format='volcano-mission-1' retail-background='103'><settings rules='rescue'/><object id='1' type='food-small' producer='9'/></mission>",
        "<mission format='volcano-mission-1' retail-background='103'><settings rules='rescue' wood='-1'/></mission>",
        "<mission format='volcano-mission-1' retail-background='103'><settings rules='rescue'/><object id='1' type='hq' colour='red'/></mission>",
        "<mission format='volcano-mission-2' retail-background='103'><settings rules='rescue'/></mission>",
    };
    for (size_t i=0;i<sizeof broken/sizeof *broken;i++) {
        char path[]="/tmp/volcano-mission-XXXXXX";int fd=mkstemp(path);
        if (fd<0) continue;
        if (write(fd,broken[i],strlen(broken[i]))<0) {}
        close(fd);
        LevelSpec spec;bool loaded=mission_load(path,&spec);
        CHECK(!loaded && strstr(last_error,path),"invalid mission %zu is rejected with its file name",i);
        if (loaded) level_spec_free(&spec);
        unlink(path);
    }
    error_clear();
}

static bool on_ellipse(Vec2 p,Vec2 centre,Vec2 axes,float tolerance)
{
    float x=(p.x-centre.x)/axes.x,y=(p.y-centre.y)/axes.y;
    return fabsf(sqrtf(x*x+y*y)-1)<=tolerance;
}

/* Dispatch to TARGET_ID and run until the worker starts working. */
static void approach_test(const char *mission,uint32_t target_id,Stock grant,const char *what)
{
    Game *g=start(mission);
    if (!g) { CHECK(false,"%s loads",mission);return; }
    int target=-1;
    for (int i=0;i<g->object_count;i++) if (g->objects[i].id==target_id) target=i;
    /* Open every other route so this test isolates the approach itself. */
    for (int i=0;i<g->object_count;i++) if (i!=target && g->objects[i].kind==BARRIER) g->objects[i].done=true;
    for (int r=0;r<RESOURCE_COUNT;r++) g->stock.amount[r]+=grant.amount[r];
    CHECK(target>=0 && game_dispatch(g,target,true),"%s: dispatch",what);
    int worker=-1;
    for (int i=0;i<g->worker_count;i++) if (g->workers[i].target==target) worker=i;
    for (int tick=0;worker>=0 && tick<20000 && g->workers[worker].phase==OUTBOUND;tick++) game_tick(g,TICK_SECONDS);
    if (worker>=0) {
        const Worker *w=&g->workers[worker];const Entity *e=&g->objects[w->entity],*t=&g->objects[target];
        Vec2 axes=game_footprint(g,t);float distance=vec2_distance(e->p,t->p);
        CHECK(w->phase==WORKING,"%s: worker reached the job",what);
        CHECK(on_ellipse(e->p,t->p,axes,.02f) || distance>=fminf(axes.x,axes.y),
              "%s: worker stops at the footprint edge (distance %.1f, axes %.1fx%.1f)",what,distance,axes.x,axes.y);
        CHECK(distance>fminf(axes.x,axes.y)*.9f,"%s: worker does not walk into the object's centre",what);
        CHECK(e->pose==POSE_WORK && e->direction==game_direction(e->p,t->p,e->type->directions),"%s: worker faces the target",what);
        printf("PASS: %s: worker works %.1f units from the centre, on its %.0fx%.0f footprint.\n",what,distance,axes.x*2,axes.y*2);
    }
    game_world_free(g);
}

int main(int argc,char **argv)
{
    if (argc!=2) { fprintf(stderr,"Usage: %s ASSET_FOLDER\n",argv[0]);return 2; }
    xml_tests();
    vfs_reset();
    if (!vfs_mount(argv[1]) || !content_load("content") || !assets_load_base()) { fprintf(stderr,"Setup: %s\n",last_error);return 1; }
    vfs_tests(argv[1]);
    CHECK(assets_selftest(),"sprite catalog and declaration parser");
    sheet_tests();
    content_tests();
    mission_tests();
    approach_test("missions/first-rescue/mission.xml",4,(Stock){{0}},"fallen tree");
    approach_test("missions/emerald-falls/mission.xml",30,(Stock){.wood=100},"damaged producer");
    approach_test("missions/library-rescue/mission.xml",23,(Stock){.food=100,.money=200},"mission debris archetype");
    assets_destroy();content_destroy();
    if (failures) { fprintf(stderr,"%d core check(s) failed\n",failures);return 1; }
    puts("PASS: core suite (XML, mounts, catalog, content, missions, headless footprint approach).");
    return 0;
}
