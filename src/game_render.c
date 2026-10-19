#include "game_internal.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct { const Entity *entity;uint32_t layer;float depth; } DrawEntry;
static DrawEntry game_draw_entry(const Game *g,const Entity *e)
{
    /* Keep bobbing and roof attachment out of the ground-position sort key. */
    return (DrawEntry){e,g->level.objects[e->source].layer,e->p.y};
}
static int draw_entry_compare(const void *a,const void *b)
{
    const DrawEntry *aa=a,*bb=b;
    if (aa->layer!=bb->layer) return aa->layer<bb->layer?-1:1;
    return aa->depth==bb->depth?aa->entity->order-bb->entity->order:aa->depth>bb->depth?-1:1;
}
typedef struct { int units[4]; } TooltipAmounts; /* Food, wood, money, fuel. */
typedef struct { TooltipAmounts required,income;int interval; } TooltipEconomy;
static const SDL_FRect resource_icons[]={
    {.9590f,.6221f,.0361f,.0352f},{.9590f,.5869f,.0400f,.0342f},
    {.9551f,.5508f,.0430f,.0342f},{.9336f,.6221f,.0225f,.0342f}};
static const float resource_icon_widths[]={23,26,28,15};

bool game_on_roof(const Entity *e) { return e->roof && e->support>=0 && !e->done && e->follower<0; }

VisualPose game_visual_pose(Game *g, const Entity *e)
{
    VisualPose pose={e->p,0};
    if (e->ambient) {
        float margin=e->ambient==BIRD?96:180,span=GAME_WIDTH+2*margin;
        double travel=g->time*e->travel_speed+game_particle_random(e->seed+3)*span;
        uint32_t pass=(uint32_t)floor(travel/span);
        pose.p.x=(float)fmod(travel,span)-margin;
        pose.p.y=70+game_particle_random(e->seed+pass*747796405u+9)*(GAME_HEIGHT-200);
        if (e->ambient==BIRD) pose.p.y+=5*sinf((float)g->time*1.2f+e->seed);
    } else if (game_on_roof(e)) {
        const Entity *house=&g->objects[e->support];VisualPose parent=game_visual_pose(g, house);
        float angle=parent.angle*(float)(PI/180),c=cosf(angle),s=sinf(angle);
        float dx=e->p.x-house->p.x,dy=e->p.y-house->p.y;
        pose.p=(Vec2){parent.p.x+c*dx+s*dy,parent.p.y-s*dx+c*dy};pose.angle=parent.angle;
    } else if (e->floating) {
        float phase=game_particle_random(e->seed)*2*(float)PI;
        float wave=(float)g->time*(1.2f+game_particle_random(e->seed+1)*.5f)+phase;
        pose.p.y+=2.3f*sinf(wave);pose.angle=1.8f*sinf(wave+.7f);
    }
    return pose;
}

int game_depth_compare(Game *g, const void *a,const void *b)
{
    const Entity *aa=*(const Entity *const *)a,*bb=*(const Entity *const *)b;
    DrawEntry first=game_draw_entry(g,aa),second=game_draw_entry(g,bb);
    return draw_entry_compare(&first,&second);
}

static void game_paint(Game *g, const Entity *e)
{
    if (!e->hidden) {
        if (e->visual>=0) {map_visual_draw(&g->scene.visuals[e->visual],g->time);return;}
        VisualPose pose=game_visual_pose(g, e);
        if (e->effect==1) {
            char path[128];snprintf(path,sizeof path,"texture/static/part_%s.png",e->anim);
            draw_image(path,pose.p.x-64,pose.p.y-32,128,96,NULL);return;
        }
        if (e->effect==2) {
            draw_particle(false,(int)(g->time*15)%32,pose.p.x,pose.p.y+18,64,0,game_color(255,255,255,255));return;
        }
        if (e->effect==3) {
            draw_sprite_tinted("water0",pose.p.x,pose.p.y,g->time,0,game_color(95,175,65,215));return;
        }
        double time=g->time+(e->ambient==BIRD?game_particle_random(e->seed)*2.4:0);
        draw_sprite_rotated(game_sprite_name(e->anim),pose.p.x,pose.p.y,time,e->frame,e->ambient==CLOUD?.65f:1,pose.angle);
    }
}

void game_paint_log_particles(Game *g, const Entity *e)
{
    if (!game_burning_log(e) || (e->hidden && !e->done) || (e->done && e->fire_stopped<0)) return;
    /* Seven seconds of birth history starts established fires fully alight.
     * No render-time state: pause, redraws and restart remain deterministic. */
    double now=g->time+7,stop=e->fire_stopped+7;
    if (e->done && now-stop>=7) return;
    float phase=game_particle_random((uint32_t)e->order+91u);
    float flicker=.88f+.08f*(float)sin(now*9+phase*19)+.04f*(float)sin(now*21+phase*37);
    float heat=e->done?fmaxf(0,1-(float)(now-stop)/.8f):1;
    if (heat>0) {
        draw_particle_glow(e->p.x,e->p.y+4,64*flicker,25,game_color(255,83,9,(int)(55*heat)));
        draw_particle_glow(e->p.x,e->p.y+9,31,20,game_color(255,153,32,(int)(70*heat)));
    }
    for (int layer=0;layer<3;layer++) {
        if (layer==1) {
            /* Two independently phased patches, each with the authored back
             * and front layers. Their roots stay on the wood while the tips
             * bend in the updraft. Cooling lifts and dissolves the last flame. */
            if (heat<=0) continue;
            for (int patch=0;patch<2;patch++) {
                float r=game_particle_random((uint32_t)e->order*17u+patch+13u);
                float wave=(float)sin(now*(5+r)+r*31);
                float width=(38+4*r)*(.8f+.2f*heat);
                float height=(80+5*r)*(1+.055f*wave);
                float x=e->p.x+(patch?8:-8)+wave*1.3f;
                float y=e->p.y+3+height*.5f+(1-heat)*8;
                int frame=(int)(now*(25+2*r)+r*40+patch*17)%40;
                draw_log_particle(false,frame,x,y,width,height,wave*3,
                    game_color(255,182,116,(int)(heat*240)));
                draw_log_particle(false,frame+40,x,y,width,height,wave*3,
                    game_color(255,236,185,(int)(heat*245)));
            }
            continue;
        }
        bool smoke=layer==0;
        double rate=smoke?2.8:2.5,max_life=smoke?7:2;
        int64_t latest=(int64_t)floor(now*rate-phase);
        for (int slot=(int)ceil(max_life*rate);slot>=0;slot--) {
            int64_t birth=latest-slot;double born=(birth+phase)/rate,age=now-born;
            if (e->done && born>stop) continue;
            uint32_t seed=(uint32_t)birth*747796405u+(uint32_t)e->order*2891336453u+(uint32_t)layer*277803737u;
            float life=(smoke?5:1.1f)+game_particle_random(seed)*(smoke?2:.9f);
            if (age<0 || age>=life) continue;
            float t=(float)(age/life),a=(float)age,r=game_particle_random(seed+1),s=game_particle_random(seed+2);
            float curl=sinf(a*(smoke?1.7f:5)+s*6.2831853f)-sinf(s*6.2831853f);
            if (smoke) {
                float x=e->p.x+(r-.5f)*23+(2+3*s)*a+curl*(2+3*t);
                float y=e->p.y+22+(13+4*r)*a+1.2f*a*a;
                float size=17+7*s+6*a;
                float opacity=fminf(1,a*1.8f)*(1-t)*(1-t)*.28f;
                int shade=(int)(103+65*t);
                draw_log_particle(true,(int)(s*4),x,y,size,size*.92f,curl*4,
                    game_color(shade+5,shade+2,shade,(int)(opacity*255)));
            } else {
                float x=e->p.x+(r-.5f)*29+(s-.35f)*12*a+curl*3*t;
                float y=e->p.y+12+(27+17*s)*a+4*a*a;
                float opacity=fminf(1,a*12)*(1-t)*(1-t);
                draw_particle_glow(x,y,2.5f*(1-t)+.6f,5*(1-t)+1,game_color(255,102,9,(int)(opacity*200)));
                draw_particle_glow(x,y,.9f,1.5f,game_color(255,216,106,(int)(opacity*245)));
            }
        }
    }
}

bool game_inside(Game *g, float x,float y,float w,float h)
{
    return g->pointer_x>=x && g->pointer_x<x+w && g->pointer_y>=y && g->pointer_y<y+h;
}

int game_hud_button(Game *g)
{
    if (g->paused || g->won) {
        if (game_inside(g, 508,429,350,83)) return 1;
        if (!(g->won && *g->custom_path) && game_inside(g, 508,329,350,55)) return 2;
    }
    return game_inside(g, 1,681,82,82)?0:-1;
}

static int game_button_state(Game *g, int button)
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

static void game_menu_button(Game *g, int id,const char *text,float y,float h,float u,float v,float uw,float vh,float cap)
{
    float uv_cap=cap/1024;v-=game_button_state(g, id)*vh;
    game_gui(508,y,cap,h,u,v,uv_cap,vh);
    game_gui(508+cap,y,350-2*cap,h,u+uv_cap,v,uw-2*uv_cap,vh);
    game_gui(858-cap,y,cap,h,u+uw-uv_cap,v,uv_cap,vh);
    game_label("CambriaBold30B",text,683,y,h,30,true);
}

static void game_draw_hud(Game *g)
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
    const int inventory[]={g->food,g->wood,g->money,g->gas};
    const float positions[]={123,211,303,383};
    char text[128];
    for (int i=0;i<4;i++) {
        snprintf(text,sizeof text,"%d",inventory[i]);
        game_label("CalibriRegular22N",text,positions[i],739,20,22,false);
    }
    game_gui(87,732,37,36,.9590f,.6221f,.0361f,.0352f);
    game_gui(173,733,41,35,.9590f,.5869f,.0400f,.0342f);
    game_gui(262,733,44,35,.9551f,.5508f,.0430f,.0342f);
    game_gui(358,733,23,35,.9336f,.6221f,.0225f,.0342f);
    const int remaining[]={g->early?g->goal-g->rescued:g->survivors-g->rescued,
        g->early?g->animals-g->animals_rescued:g->barriers-g->cleared,
        g->early?g->basin_goal-g->purified:g->resources-g->collected};
    const char *tasks[]={"Rescue people",g->early?"Rescue animals":"Clear obstructions",g->early?"Purify the basins":"Collect supplies"};
    int row=0;
    for (int i=0;i<3;i++) {
        if (g->early && ((i==1 && !g->animals) || (i==2 && !g->basin_goal))) continue;
        float y=741-21*row;bool done=remaining[i]==0;
        if (done) game_gui(1036,y,21,22,.8535f,.7373f,.0205f,.0215f);
        else game_gui(1035,y,20,21,.8330f,.7373f,.0195f,.0205f);
        if (done) snprintf(text,sizeof text,"%s: Done!",tasks[i]);
        else snprintf(text,sizeof text,"%s: %d",tasks[i],remaining[i]);
        game_label(done?"CalibriBold20G":"CalibriBold20W",text,1059,738-21*row++,25,20,false);
    }
    if (g->early) {
        if (*g->custom_path) snprintf(text,sizeof text,"Custom level   Time %d:%02d",(int)g->time/60,(int)g->time%60);
        else snprintf(text,sizeof text,"Level %d   Time %d:%02d",g->campaign,(int)g->time/60,(int)g->time%60);
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
        game_menu_button(g, 1,g->won?(*g->custom_path?"Play again":"Next level"):"Continue",429,83,0,.9189f,.2676f,.0811f,45);
        if (!(g->won && *g->custom_path)) game_menu_button(g, 2,"Play again",329,55,.5059f,.9463f,.2363f,.0537f,32);
    }
    /* Context feedback is transient; it does not cover the scene with a bar. */
    if (!g->paused && g->message_time>0) {
        float width=text_width("CalibriBold20W",g->message,20);
        draw_rect((GAME_WIDTH-width)*.5f-14,GAME_HEIGHT-42,width+28,30,game_color(10,28,40,205));
        draw_font("CalibriBold20W",g->message,(GAME_WIDTH-width)*.5f,GAME_HEIGHT-40,20,game_color(255,255,255,255));
    }
}

static bool game_over_hud(Game *g) { return game_inside(g, 0,668,446,100) || game_inside(g, 1010,671,356,97); }

int game_hovered_object(Game *g)
{
    if (g->paused || g->won || game_over_hud(g)) return -1;
    Vec2 p={g->pointer_x,g->pointer_y};int best=-1;float shortest=INFINITY;
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (e->hidden || !(game_job(g, e) || e->kind==BUILD_LOT || e->kind==BUILDING || e->kind==BASE || e->kind==RECRUIT || e->kind==SERVICE || e->kind==VEHICLE) || (game_job(g, e) && e->done && e->kind!=BUILDING)) continue;
        Animation *a=animation_find(game_sprite_name(e->anim));if ((!a || !a->frame_count) && !e->effect) continue;
        SpriteFrame f=e->effect?(SpriteFrame){.dx=40,.dy=20,.w=80,.h=64}:sprite_body_frame(a->frames[0]);VisualPose pose=game_visual_pose(g, e);
        float angle=pose.angle*(float)(PI/180),c=cosf(angle),s=sinf(angle);
        float dx=p.x-pose.p.x,dy=p.y-pose.p.y,lx=c*dx-s*dy,ly=s*dx+c*dy;
        if (lx>=-f.dx-6 && lx<=f.w-f.dx+6 && ly>=-f.dy-6 && ly<=f.h-f.dy+6) {
            float d=game_distance(p,pose.p);if (d<shortest) best=i,shortest=d;
        }
    }
    return best;
}

Vec2 game_build_button_position(Game *g, int option)
{
    Entity *e=&g->objects[g->build_menu];
    float x=fmaxf(176,fminf(GAME_WIDTH-176,e->p.x));
    float y=fmaxf(120,fminf(570,e->p.y-90));
    const Vec2 offsets[]={{-105,34},{-35,-24},{35,-24},{105,34}};
    return (Vec2){x+offsets[option].x,y+offsets[option].y};
}

int game_build_button_at(Game *g)
{
    if (g->build_menu<0 || g->paused) return -1;
    if (g->early && g->objects[g->build_menu].kind==BUILDING)
        return game_distance((Vec2){g->pointer_x,g->pointer_y},game_build_button_position(g, 2))<39?4:-1;
    for (int i=0;i<4;i++) if (game_distance((Vec2){g->pointer_x,g->pointer_y},game_build_button_position(g, i))<39) return i;
    return -1;
}

static SDL_FRect game_tooltip_frame(Game *g, const char *title,float content_width,int rows)
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

static void game_tooltip(Game *g, const char *title,const char *detail)
{
    SDL_FRect r=game_tooltip_frame(g, title,text_width("CalibriBold20W",detail,20),*detail?1:0);
    if (*detail) game_label("CalibriBold20W",detail,r.x+r.w*.5f,r.y+7,25,20,true);
}

static bool game_has_amounts(TooltipAmounts a) { return a.units[0] || a.units[1] || a.units[2] || a.units[3]; }

static TooltipEconomy game_entity_economy(Game *g, const Entity *e)
{
    Cost c=game_job_cost(g, e);
    TooltipEconomy t={.required={{c.food,c.wood,c.money,0}}};
    if (g->early && e->transport!=ON_FOOT) t.required.units[3]=10;
    if (e->kind==RESOURCE) {
        static const int slots[]={0,0,3,2,1};
        t.income.units[slots[e->pickup_resource]]=e->pickup_value;
    } else if (g->early && e->kind==BARRIER && e->obstacle) t.income.units[1]=e->obstacle->wood_reward;
    else if (g->early && (e->kind==BUILDING || (e->kind==BUILD_LOT && e->assigned))) {
        static const int slots[]={0,0,3,2,1};
        t.income.units[slots[game_buildings[e->building].resource]]=PRODUCTION_INCOME;t.interval=PRODUCTION_INTERVAL;
        if (e->kind==BUILD_LOT) t.required.units[1]=game_buildings[e->building].cost;
    }
    return t;
}

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

static void game_economy_tooltip(Game *g, const char *title,TooltipEconomy t)
{
    bool required=game_has_amounts(t.required),income=game_has_amounts(t.income);
    float w=required?game_amount_line("Required: ",t.required,0,0,0,false):0;
    if (income) w=fmaxf(w,game_amount_line("Income: ",t.income,t.interval,0,0,false));
    SDL_FRect r=game_tooltip_frame(g, title,w,required+income);float y=r.y+r.h-55;
    if (required) { game_amount_line("Required: ",t.required,0,r.x+r.w*.5f,y,true);y-=25; }
    if (income) game_amount_line("Income: ",t.income,t.interval,r.x+r.w*.5f,y,true);
}

static const char *game_entity_title(Game *g, const Entity *e)
{
    if (e->kind==BUILD_LOT) return e->assigned?game_buildings[e->building].name:"Building slot";
    if (e->kind==BASE) return "Headquarters";
    if (e->kind==ANIMAL) return !strcmp(g->level.objects[e->source].params,"cat")?"Rescue a cat":"Rescue a dog";
    if (e->kind==VALVE) return "Valve";
    if (e->kind==SERVICE) return g->level.objects[e->source].type==114?"Pier":"Garage";
    if (e->kind==VEHICLE) return g->level.objects[e->source].type==113?"Fishing boat":"ATV";
    if (e->kind==RECRUIT) return "Trapped worker";
    if (e->kind==SURVIVOR) return "Survivor";
    if (strstr(e->anim,"stone")) return "Stone";
    if (strstr(e->anim,"tree")) return "Fallen tree";
    if (strstr(e->anim,"bridge")) return "Broken bridge";
    if (strstr(e->anim,"road")) return "Damaged road";
    if (strstr(e->anim,"house")) return "House";
    if (strstr(e->anim,"sawmill")) return "Sawmill";
    if (strstr(e->anim,"burger")) return "Burger Cafe";
    if (strstr(e->anim,"food")) return "Food";
    if (strstr(e->anim,"money")) return "Money";
    if (strstr(e->anim,"gas")) return "Fuel";
    if (strstr(e->anim,"wood")) return "Wood";
    return "Building";
}

static void game_draw_context(Game *g)
{
    if (g->paused) return;
    if (g->build_menu>=0) {
        if (g->early && g->objects[g->build_menu].kind==BUILDING) {
            Vec2 p=game_build_button_position(g, 2);int state=game_build_button_at(g)==4?(g->pointer_down?2:1):0;
            game_gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-26,p.y-32,52,65},(SDL_FRect){.9004f,.0078f,.0508f,.5078f});
            if (game_build_button_at(g)==4) game_tooltip(g, "Demolish building","");
            return;
        }
        static const SDL_FRect images[]={{.7891f,.0078f,.0615f,.4063f},{.6777f,.0078f,.0566f,.3906f},
            {.7354f,.0078f,.0518f,.4141f},{.6230f,.0156f,.0547f,.3750f}};
        static const SDL_FRect locks[]={{.8242f,.4375f,.0605f,.4063f},{.7129f,.4375f,.0557f,.3906f},
            {.7695f,.4375f,.0518f,.4141f},{.6572f,.4453f,.0547f,.3750f}};
        int hover=game_build_button_at(g);char text[80];
        for (int i=0;i<4;i++) {
            Vec2 p=game_build_button_position(g, i);bool afford=g->wood>=game_buildings[i].cost && game_allowed_building(g, i);
            int state=hover==i?(g->pointer_down?2:1):0;
            game_gui(p.x-41,p.y-41,82,82,.7441f,.9189f-state*.0801f,.0801f,.0801f);
            draw_atlas("texture/gui/gui01.png",(SDL_FRect){p.x-30,p.y-13,60,50},afford?images[i]:locks[i]);
            snprintf(text,sizeof text,"%d",game_buildings[i].cost);
            draw_font("CalibriBold20W",text,p.x-8-text_width("CalibriBold20W",text,20)*.5f,
                GAME_HEIGHT-p.y+13.5f,20,afford?game_color(255,255,255,255):game_color(255,85,75,255));
            game_gui(p.x+9,p.y-34,24,21,.9590f,.5869f,.0400f,.0342f);
        }
        if (hover>=0) {
            if (!game_allowed_building(g, hover)) game_tooltip(g, game_buildings[hover].name,"Not available on this level");
            else {
                Entity planned={.kind=BUILD_LOT,.assigned=true,.building=hover};
                game_economy_tooltip(g, game_buildings[hover].name,game_entity_economy(g, &planned));
            }
        }
        return;
    }
    int hover=game_hovered_object(g);
    if (hover>=0) game_economy_tooltip(g, game_entity_title(g, &g->objects[hover]),game_entity_economy(g, &g->objects[hover]));
}

static void game_draw_progress(Game *g, const Entity *target,double fraction)
{
    VisualPose pose=game_visual_pose(g, target);float x=pose.p.x,y=pose.p.y-37;
    draw_sprite("progress",x,y,0,0,1);
    SDL_Rect previous;bool clipped=SDL_RenderIsClipEnabled(renderer);SDL_RenderGetClipRect(renderer,&previous);
    SDL_Rect clip={(int)(x-33),(int)(GAME_HEIGHT-y-6),(int)(66*fmax(0,fmin(1,fraction))),12};
    SDL_RenderSetClipRect(renderer,&clip);draw_sprite("progress",x,y,0,1,1);
    SDL_RenderSetClipRect(renderer,clipped?&previous:NULL);
}

void game_render(Game *g)
{
    if (!g || !g->ready) return;
    for (int i=0;i<g->object_count;i++) if (g->objects[i].kind==WATER && g->objects[i].effect!=3) game_paint(g, &g->objects[i]);
    for (int i=0;i<g->object_count;i++) if (g->objects[i].kind==WATER && g->objects[i].effect==3) game_paint(g, &g->objects[i]);
    if (g->local_background) draw_image_file(g->background,0,0,GAME_WIDTH,GAME_HEIGHT);
    else draw_image(g->background,0,0,GAME_WIDTH,GAME_HEIGHT,g->alpha);
    map_scene_draw(&g->scene,MAP_GROUND,g->time);
    if (g->debug) {
        for (int i=0;i<g->level.traffic_count;i++) for (int j=i+1;j<g->level.traffic_count;j++) {
            Vec2 a=g->level.traffic[i],b=g->level.traffic[j];
            if (game_distance(a,b)<50) draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,game_blocked(g, a,b,-1)?game_color(255,70,100,180):game_color(70,220,100,180));
        }
    }
    int hover=game_hovered_object(g);
    if (!g->paused && (hover>=0 || g->build_menu>=0)) {
        int selected=g->build_menu>=0?g->build_menu:hover;Entity *e=&g->objects[selected];
        Animation *a=animation_find(game_sprite_name(e->anim));float w=a?fmaxf(65,sprite_body_frame(a->frames[0]).w+24):100;
        w=fminf(w,170);float h=w*.62f;
        int state=(e->kind==BASE || game_reachable(g, selected)) && game_affordable(g, game_job_cost(g, e)) &&
            (!g->early || e->transport==ON_FOOT || g->gas>=10)?1:0;
        VisualPose pose=game_visual_pose(g, e);
        draw_atlas("texture/gui/highlight.png",(SDL_FRect){pose.p.x-w*.5f,pose.p.y-h*.5f,w,h},
            (SDL_FRect){.0078125f,state?.140625f:.5703125f,.6171875f,.421875f});
    }
    DrawEntry sorted[MAX_LEVEL_OBJECTS];int count=0;
    for (int i=0;i<g->object_count;i++) if (g->objects[i].kind!=WATER) sorted[count++]=game_draw_entry(g,&g->objects[i]);
    qsort(sorted,count,sizeof(*sorted),draw_entry_compare);
    for (int i=0;i<count;i++) {
        const Entity *e=sorted[i].entity;if (e->hidden) { game_paint_log_particles(g, e);continue; }
        if ((e->kind==WORKER || e->kind==RECRUIT) && strncmp(e->anim,"boat",4)) draw_sprite("worker_shadow",e->p.x,e->p.y,g->time,-1,1);
        if (e->kind==SURVIVOR && !game_on_roof(e)) draw_sprite("man_shadow",e->p.x,e->p.y,g->time,-1,1);
        game_paint(g, e);
        game_paint_log_particles(g, e);
    }
    for (int i=0;i<g->worker_count;i++) {
        Worker *w=&g->workers[i];Entity *e=&g->objects[w->entity];
        if (w->phase==WORKING) {
            Entity *t=&g->objects[w->target];game_draw_progress(g, t,w->work/t->duration);
        }
        if (g->debug && (w->phase==OUTBOUND || w->phase==RETURNING)) for (int j=w->step;j<w->count;j++) {
            Vec2 a=j==w->step?e->p:w->path[j-1],b=w->path[j];draw_line(a.x,GAME_HEIGHT-a.y,b.x,GAME_HEIGHT-b.y,game_color(255,224,90,255));
        }
    }
    for (int i=0;i<g->object_count;i++) {
        Entity *e=&g->objects[i];
        if (!e->hidden && (e->queued || e->assigned)) {
            bool working=false;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].target==i && g->workers[j].phase==WORKING) working=true;
            VisualPose pose=game_visual_pose(g, e);
            if (!working) draw_sprite("target",pose.p.x+22,pose.p.y-12,0,0,1);
        }
        if (e->kind==BASE) {
            int count=0,total=0;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].home_base==i && g->workers[j].phase==IDLE) total++;
            for (int j=0;j<g->worker_count;j++) if (g->workers[j].home_base==i && g->workers[j].phase==IDLE)
                draw_sprite("icon_head",e->p.x+(count++-(total-1)*.5f)*23,e->p.y+65,0,0,1);
        }
    }
    map_scene_draw(&g->scene,MAP_OVERLAY,g->time);
    game_draw_context(g);game_draw_hud(g);
}
