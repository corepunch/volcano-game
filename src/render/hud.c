/* HUD, pause panel, construction menu, tooltips and picking. GUI
 * rectangles and UVs are recovered from retail gui/level.lua and
 * gui/escape.lua (1366 x 768, bottom-left origin). */
#include "render.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { int units[RESOURCE_COUNT]; } TooltipAmounts;
typedef struct { TooltipAmounts required,income;int interval; } TooltipEconomy;
static const SDL_FRect resource_icons[RESOURCE_COUNT]={
    {.9590f,.6221f,.0361f,.0352f},{.9590f,.5869f,.0400f,.0342f},
    {.9551f,.5508f,.0430f,.0342f},{.9336f,.6221f,.0225f,.0342f}};
static const float resource_icon_widths[RESOURCE_COUNT]={23,26,28,15};

/* Missions loaded from files replay instead of advancing the campaign. */
static bool mission_file(const Game *g) { return g->spec.source_index<0; }

bool game_inside(const Game *g,float x,float y,float w,float h)
{
    return g->pointer_x>=x && g->pointer_x<x+w && g->pointer_y>=y && g->pointer_y<y+h;
}

int game_hud_button(const Game *g)
{
    if (g->paused || g->won) {
        if (game_inside(g, 508,429,350,83)) return 1;
        if (!(g->won && mission_file(g)) && game_inside(g, 508,329,350,55)) return 2;
    }
    return game_inside(g, 1,681,82,82)?0:-1;
}

static int game_button_state(const Game *g, int button)
{
    return game_hud_button(g)==button?(g->pointer_down?2:1):0;
}

static void game_gui(float x,float y,float w,float h,float u,float v,float uw,float vh)
{
    draw_atlas("texture/gui/gui00.png",(SDL_FRect){x,y,w,h},(SDL_FRect){u,v,uw,vh});
}

static void game_label(const char *font,const char *text,float x,float y,float h,int size,bool centered)
{
    if (centered) x-=text_width(font,text,size)*0.5f;
    draw_font(font,text,x,GAME_HEIGHT-y-h+(h-size)*0.5f,size,game_color(255,255,255,255));
}

static void game_menu_button(const Game *g, int id,const char *text,float y,float h,float u,float v,float uw,float vh,float cap)
{
    float uv_cap=cap/1024;v-=game_button_state(g, id)*vh;
    game_gui(508,y,cap,h,u,v,uv_cap,vh);
    game_gui(508+cap,y,350-2*cap,h,u+uv_cap,v,uw-2*uv_cap,vh);
    game_gui(858-cap,y,cap,h,u+uw-uv_cap,v,uv_cap,vh);
    game_label("CambriaBold30B",text,683,y,h,30,true);
}

void game_draw_hud(Game *g)
{
    /* Rectangles/UVs recovered from retail gui/level.lua (1366 x 768). */
    game_gui(-47,668,488,115,.0020f,.5332f,.4766f,.1123f);
    game_gui(1,681,82,82,.7441f,.9189f-game_button_state(g, 0)*.0801f,.0801f,.0801f);
    draw_atlas("texture/gui/gui01.png",(SDL_FRect){24,704,36,37},(SDL_FRect){.6201f,.7031f,.0352f,.2891f});
    game_gui(1010,671,454,153,.4805f,.5176f,.4434f,.1494f);
    game_gui(87,707,359,28,.0020f,.6504f,.3506f,.0273f);
    /* The restored prototype is untimed: keep the retail time meter full.
       The supplied .lvl data has no retail time-limit configuration. */
    game_gui(89,715,352,12,.3662f,.6719f,.3438f,.0117f);
    game_gui(200,708,10,26,.3545f,.6514f,.0098f,.0254f);
    const int *inventory=g->stock.amount;
    const float positions[]={123,211,303,383};
    char text[256];
    for (int i=0;i<4;i++) {
        snprintf(text,sizeof text,"%d",inventory[i]);
        game_label("CalibriRegular22N",text,positions[i],739,20,22,false);
    }
    game_gui(87,732,37,36,.9590f,.6221f,.0361f,.0352f);
    game_gui(173,733,41,35,.9590f,.5869f,.0400f,.0342f);
    game_gui(262,733,44,35,.9551f,.5508f,.0430f,.0342f);
    game_gui(358,733,23,35,.9336f,.6221f,.0225f,.0342f);
    bool goals=g->rules->goals;
    const int remaining[]={goals?g->goal-g->rescued:g->survivors-g->rescued,
        goals?g->animals-g->animals_rescued:g->barriers-g->cleared,
        goals?g->basin_goal-g->purified:g->resources-g->collected};
    const char *tasks[]={"Rescue people",goals?"Rescue animals":"Clear obstructions",goals?"Purify the basins":"Collect supplies"};
    int row=0;
    for (int i=0;i<3;i++) {
        if (goals && ((i==1 && !g->animals) || (i==2 && !g->basin_goal))) continue;
        float y=741-21*row;bool done=remaining[i]==0;
        if (done) game_gui(1036,y,21,22,.8535f,.7373f,.0205f,.0215f);
        else game_gui(1035,y,20,21,.8330f,.7373f,.0195f,.0205f);
        if (done) snprintf(text,sizeof text,"%s: Done!",tasks[i]);
        else snprintf(text,sizeof text,"%s: %d",tasks[i],remaining[i]);
        game_label(done?"CalibriBold20G":"CalibriBold20W",text,1059,738-21*row++,25,20,false);
    }
    if (goals) {
        snprintf(text,sizeof text,"%s   Time %d:%02d",g->spec.title,(int)g->time/60,(int)g->time%60);
        game_label("CalibriBold20W",text,683,718,25,20,true);
    }
    if (g->paused || g->won) {
        draw_rect(0,0,GAME_WIDTH,GAME_HEIGHT,game_color(0,0,0,130));
        /* Retail pause panel and header from gui/escape.lua. */
        game_gui(367,154,632,461,0,0,.6172f,.4502f);
        game_gui(378,542,610,64,.0029f,.4502f,.5957f,.0625f);
        const char *title=g->won?"Level complete":"Pause";
        draw_font("CambriaBold30W",title,683-text_width("CambriaBold30W",title,30)*.5f,
            GAME_HEIGHT-554-45+7.5f,30,game_color(110,83,58,255));
        game_menu_button(g, 1,g->won?(mission_file(g)?"Play again":"Next level"):"Continue",429,83,0,.9189f,.2676f,.0811f,45);
        if (!(g->won && mission_file(g))) game_menu_button(g, 2,"Play again",329,55,.5059f,.9463f,.2363f,.0537f,32);
    }
    /* Context feedback is transient; it does not cover the scene with a bar. */
    if (!g->paused && g->message_time>0) {
        float width=text_width("CalibriBold20W",g->message,20);
        draw_rect((GAME_WIDTH-width)*.5f-14,GAME_HEIGHT-42,width+28,30,game_color(10,28,40,205));
        draw_font("CalibriBold20W",g->message,(GAME_WIDTH-width)*.5f,GAME_HEIGHT-40,20,game_color(255,255,255,255));
    }
}

static SDL_FRect game_tooltip_frame(const Game *g, const char *title,float content_width,int rows)
{
    float w=fmaxf(text_width("CalibriBold20W",title,20),content_width)+28;
    float h=39+25*rows,x=fminf(GAME_WIDTH-w-8,fmaxf(8,g->pointer_x+22));
    float y=fminf(GAME_HEIGHT-h-8,fmaxf(8,g->pointer_y-h-20));
    /* Hint window's original nine-slice artwork, with ten-pixel end caps. */
    const float us[]={.6748f,.6846f,.9873f},vs[]={.2314f,.2412f,.3466f};
    const float uw[]={.0098f,.3027f,.0098f},vh[]={.0098f,.1054f,.0098f};
    float xs[]={x,x+10,x+w-10},ys[]={y,y+10,y+h-10};
    float ws[]={10,w-20,10},hs[]={10,h-20,10};
    for (int j=0;j<3;j++) for (int i=0;i<3;i++) game_gui(xs[i],ys[j],ws[i],hs[j],us[i],vs[j],uw[i],vh[j]);
    game_label("CalibriBold20W",title,x+w*.5f,y+h-30,25,20,true);
    return (SDL_FRect){x,y,w,h};
}

static void game_tooltip(const Game *g, const char *title,const char *detail)
{
    SDL_FRect r=game_tooltip_frame(g, title,text_width("CalibriBold20W",detail,20),*detail?1:0);
    if (*detail) game_label("CalibriBold20W",detail,r.x+r.w*.5f,r.y+7,25,20,true);
}

static bool game_has_amounts(TooltipAmounts a) { return a.units[0] || a.units[1] || a.units[2] || a.units[3]; }

static float game_amount_line(const char *prefix,TooltipAmounts amounts,int interval,float center,float y,bool draw)
{
    char digits[32],tail[32]="";float width=text_width("CalibriBold20W",prefix,20);int count=0;
    if (interval) snprintf(tail,sizeof tail," / %ds",interval);
    for (int i=0;i<4;i++) if (amounts.units[i]) {
        snprintf(digits,sizeof digits,"%d",amounts.units[i]);
        width+=(count++?8:0)+text_width("CalibriBold20W",digits,20)+3+resource_icon_widths[i];
    }
    width+=text_width("CalibriBold20W",tail,20);
    if (!draw) return width;
    float x=center-width*.5f;game_label("CalibriBold20W",prefix,x,y,25,20,false);x+=text_width("CalibriBold20W",prefix,20);count=0;
    for (int i=0;i<4;i++) if (amounts.units[i]) {
        if (count++) x+=8;
        snprintf(digits,sizeof digits,"%d",amounts.units[i]);game_label("CalibriBold20W",digits,x,y,25,20,false);
        x+=text_width("CalibriBold20W",digits,20)+3;
        draw_atlas("texture/gui/gui00.png",(SDL_FRect){x,y+1,resource_icon_widths[i],23},resource_icons[i]);
        x+=resource_icon_widths[i];
    }
    if (*tail) game_label("CalibriBold20W",tail,x,y,25,20,false);
    return width;
}

static void game_economy_tooltip(const Game *g, const char *title,TooltipEconomy t)
{
    bool required=game_has_amounts(t.required),income=game_has_amounts(t.income);
    float w=required?game_amount_line("Required: ",t.required,0,0,0,false):0;
    if (income) w=fmaxf(w,game_amount_line("Income: ",t.income,t.interval,0,0,false));
    SDL_FRect r=game_tooltip_frame(g, title,w,required+income);float y=r.y+r.h-55;
    if (required) { game_amount_line("Required: ",t.required,0,r.x+r.w*.5f,y,true);y-=25; }
    if (income) game_amount_line("Income: ",t.income,t.interval,r.x+r.w*.5f,y,true);
}

static bool game_over_hud(const Game *g) { return game_inside(g,0,668,446,100) || game_inside(g,1010,671,356,97); }

static bool hoverable(const Game *g,const Entity *e)
{
    bool job=game_job(g,e);
    if (e->hidden || (job && e->done && e->kind!=BUILDING)) return false;
    return job || e->kind==BUILD_LOT || e->kind==BUILDING || e->kind==BASE || e->kind==RECRUIT || e->kind==SERVICE || e->kind==VEHICLE;
}

int game_hovered_object(const Game *g)
{
    if (g->paused || g->won || game_over_hud(g)) return -1;
    Vec2 p={g->pointer_x,g->pointer_y};int best=-1;float shortest=INFINITY;
    for (int i=0;i<g->object_count;i++) {
        const Entity *e=&g->objects[i];
        if (!hoverable(g,e)) continue;
        SpriteFrame f;char name[160];const Sprite *s=NULL;
        if (e->type->effect!=EFFECT_NONE) f=(SpriteFrame){.dx=40,.dy=20,.w=80,.h=64};
        else if (game_entity_sprite(g,e,name,sizeof name) && (s=sprite_find(name)) && s->frame_count) f=sprite_body_frame(s->frames[0]);
        else continue;
        VisualPose pose=game_visual_pose(g,e);
        Vec2 origin=archetype_sprite_origin(e->type,s,pose.p);
        float angle=pose.angle*(float)(PI/180),c=cosf(angle),sn=sinf(angle);
        float dx=p.x-origin.x,dy=p.y-origin.y,lx=c*dx-sn*dy,ly=sn*dx+c*dy;
        if (lx>=-f.dx-6 && lx<=f.w-f.dx+6 && ly>=-f.dy-6 && ly<=f.h-f.dy+6) {
            float d=vec2_distance(p,pose.p);
            if (d<shortest) best=i,shortest=d;
        }
    }
    return best;
}

Vec2 game_build_button_position(const Game *g,int option)
{
    const Entity *e=&g->objects[g->build_menu];
    float x=fmaxf(176,fminf(GAME_WIDTH-176,e->p.x));
    float y=fmaxf(120,fminf(570,e->p.y-90));
    static const Vec2 offsets[]={{-105,34},{-35,-24},{35,-24},{105,34}};
    return vec2(x+offsets[option].x,y+offsets[option].y);
}

static bool demolition_menu(const Game *g) { return g->rules->economy && g->objects[g->build_menu].kind==BUILDING; }

int game_build_button_at(const Game *g)
{
    if (g->build_menu<0 || g->paused) return -1;
    Vec2 pointer={g->pointer_x,g->pointer_y};
    if (demolition_menu(g)) return vec2_distance(pointer,game_build_button_position(g,2))<39?BUILD_OPTION_DEMOLISH:-1;
    const Archetype *menu[4];int count=content_menu(menu,4);
    for (int i=0;i<count;i++) if (vec2_distance(pointer,game_build_button_position(g,i))<39) return i;
    return -1;
}

static TooltipEconomy entity_economy(const Game *g,const Entity *e)
{
    Stock c=game_job_cost(g,e);TooltipEconomy t={0};
    for (int r=0;r<RESOURCE_COUNT;r++) t.required.units[r]=c.amount[r];
    if (g->rules->vehicles && e->transport!=TRANSPORT_FOOT) t.required.units[RESOURCE_FUEL]=10;
    if (e->kind==RESOURCE) t.income.units[e->type->resource]=e->type->amount;
    else if (g->rules->economy && e->kind==BARRIER) for (int r=0;r<RESOURCE_COUNT;r++) t.income.units[r]=e->type->reward.amount[r];
    else if (g->rules->economy && (e->kind==BUILDING || (e->kind==BUILD_LOT && e->assigned && e->building))) {
        const Archetype *producer=e->kind==BUILDING?e->type:e->building;
        t.income.units[producer->produces]=producer->income;t.interval=(int)producer->income_interval;
        if (e->kind==BUILD_LOT) for (int r=0;r<RESOURCE_COUNT;r++) t.required.units[r]=producer->build_cost.amount[r];
    }
    return t;
}

void game_draw_context(Game *g)
{
    if (g->paused) return;
    if (g->build_menu>=0) {
        if (demolition_menu(g)) {
            Vec2 p=game_build_button_position(g,2);int state=game_build_button_at(g)==BUILD_OPTION_DEMOLISH?(g->pointer_down?2:1):0;
            game_gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-26,p.y-32,52,65},(SDL_FRect){.9004f,.0078f,.0508f,.5078f});
            if (game_build_button_at(g)==BUILD_OPTION_DEMOLISH) game_tooltip(g,"Demolish building","");
            return;
        }
        /* Retail menu icons by slot: sawmill, cafe, house, gas station. */
        static const SDL_FRect images[]={{.7891f,.0078f,.0615f,.4063f},{.6777f,.0078f,.0566f,.3906f},
            {.7354f,.0078f,.0518f,.4141f},{.6230f,.0156f,.0547f,.3750f}};
        static const SDL_FRect locks[]={{.8242f,.4375f,.0605f,.4063f},{.7129f,.4375f,.0557f,.3906f},
            {.7695f,.4375f,.0518f,.4141f},{.6572f,.4453f,.0547f,.3750f}};
        const Archetype *menu[4];int count=content_menu(menu,4),hover=game_build_button_at(g);char text[80];
        for (int i=0;i<count;i++) {
            Vec2 p=game_build_button_position(g,i);
            bool afford=game_affordable(g,menu[i]->build_cost) && mission_allows(&g->mission,menu[i]);
            int state=hover==i?(g->pointer_down?2:1):0;
            game_gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-30,p.y-13,60,50},afford?images[i]:locks[i]);
            snprintf(text,sizeof text,"%d",menu[i]->build_cost.wood);
            draw_font("CalibriBold20W",text,p.x-8-text_width("CalibriBold20W",text,20)*.5f,
                GAME_HEIGHT-p.y+13.5f,20,afford?game_color(255,255,255,255):game_color(255,85,75,255));
            game_gui(p.x+9,p.y-34,24,21,.9590f,.5869f,.0400f,.0342f);
        }
        if (hover>=0 && hover<count) {
            if (!mission_allows(&g->mission,menu[hover])) game_tooltip(g,menu[hover]->title,"Not available on this level");
            else {
                Entity planned={.kind=BUILD_LOT,.type=g->lot,.assigned=true,.building=menu[hover],.transport=TRANSPORT_FOOT};
                game_economy_tooltip(g,menu[hover]->title,entity_economy(g,&planned));
            }
        }
        return;
    }
    int hover=game_hovered_object(g);
    if (hover>=0) {
        const Entity *e=&g->objects[hover];
        game_economy_tooltip(g,e->kind==RECRUIT?"Trapped worker":game_entity_title(g,e),entity_economy(g,e));
    }
}
