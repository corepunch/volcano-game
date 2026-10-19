/* Importer for retail Rescue Team atlas (texture/lazy/NAME.lua) and bitmap font
 * declarations. A data lexer, deliberately not a Lua interpreter: only scalar
 * assignments, records and the known declaration calls are accepted. */
#include "assets_internal.h"
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { TOKEN_END=256,TOKEN_WORD,TOKEN_STRING,TOKEN_NUMBER };
typedef struct {
    const char *source,*at,*end,*path;
    int token,line;
    char text[PATH_CAPACITY];
    double number;
    bool failed;
} Lexer;

static size_t utf8_write(char *out,uint32_t c)
{
    if (c<0x80) { out[0]=(char)c;return 1; }
    if (c<0x800) { out[0]=(char)(0xC0|c>>6);out[1]=(char)(0x80|(c&63));return 2; }
    if (c<0x10000) { out[0]=(char)(0xE0|c>>12);out[1]=(char)(0x80|(c>>6&63));out[2]=(char)(0x80|(c&63));return 3; }
    out[0]=(char)(0xF0|c>>18);out[1]=(char)(0x80|(c>>12&63));out[2]=(char)(0x80|(c>>6&63));out[3]=(char)(0x80|(c&63));return 4;
}

char *declaration_text(const char *path,size_t *length)
{
    size_t got;unsigned char *bytes=(unsigned char *)file_read(path,32u<<20,&got);
    if (!bytes) return NULL;
    if (got>=2 && ((bytes[0]==255 && bytes[1]==254) || (bytes[0]==254 && bytes[1]==255))) {
        bool little=bytes[0]==255;
        if (got%2) { free(bytes);error_set("Odd UTF-16 resource length: %s",path);return NULL; }
        char *decoded=malloc(got*2+1);if (!decoded) { free(bytes);error_set("Out of memory");return NULL; }
        size_t count=0;
        for (size_t i=2;i<got;i+=2) {
            uint32_t c=little?(uint32_t)(bytes[i]|bytes[i+1]<<8):(uint32_t)(bytes[i]<<8|bytes[i+1]);
            if (c>=0xD800 && c<=0xDBFF) {
                if (i+3>=got) { free(decoded);free(bytes);error_set("Truncated UTF-16 surrogate: %s",path);return NULL; }
                uint32_t low=little?(uint32_t)(bytes[i+2]|bytes[i+3]<<8):(uint32_t)(bytes[i+2]<<8|bytes[i+3]);
                if (low<0xDC00 || low>0xDFFF) { free(decoded);free(bytes);error_set("Invalid UTF-16 surrogate: %s",path);return NULL; }
                c=0x10000+((c-0xD800)<<10)+low-0xDC00;i+=2;
            } else if (c>=0xDC00 && c<=0xDFFF) { free(decoded);free(bytes);error_set("Invalid UTF-16 surrogate: %s",path);return NULL; }
            count+=utf8_write(decoded+count,c);
        }
        decoded[count]=0;free(bytes);if (length) *length=count;return decoded;
    }
    if (got>=3 && bytes[0]==0xEF && bytes[1]==0xBB && bytes[2]==0xBF) { memmove(bytes,bytes+3,got-3);got-=3;bytes[got]=0; }
    if (length) *length=got;
    return (char *)bytes;
}

static void lexer_error(Lexer *l,const char *message)
{
    if (!l->failed) error_set("%s:%d: %s near '%.64s'",l->path,l->line,message,l->text);
    l->failed=true;l->token=TOKEN_END;l->at=l->end;
}

static void next(Lexer *l)
{
    if (l->failed) return;
    for (;;) {
        while (l->at<l->end && isspace((unsigned char)*l->at)) { if (*l->at=='\n') l->line++;l->at++; }
        if (l->end-l->at>=2 && l->at[0]=='-' && l->at[1]=='-') {
            l->at+=2;
            if (l->end-l->at>=2 && l->at[0]=='[' && l->at[1]=='[') {
                l->at+=2;
                while (l->end-l->at>=2 && !(l->at[0]==']' && l->at[1]==']')) { if (*l->at=='\n') l->line++;l->at++; }
                if (l->end-l->at<2) { lexer_error(l,"Unterminated comment");return; }
                l->at+=2;
            } else while (l->at<l->end && *l->at!='\n') l->at++;
        } else break;
    }
    l->text[0]=0;
    if (l->at>=l->end) { l->token=TOKEN_END;return; }
    unsigned char c=(unsigned char)*l->at++;
    if (isalpha(c) || c=='_') {
        const char *start=l->at-1;
        while (l->at<l->end && (isalnum((unsigned char)*l->at) || *l->at=='_')) l->at++;
        size_t n=(size_t)(l->at-start);if (n>=sizeof l->text) { lexer_error(l,"Identifier too long");return; }
        memcpy(l->text,start,n);l->text[n]=0;l->token=TOKEN_WORD;return;
    }
    if (c=='\'' || c=='"') {
        size_t n=0;
        while (l->at<l->end && (unsigned char)*l->at!=c) {
            unsigned char v=(unsigned char)*l->at++;
            if (v=='\\') {
                if (l->at==l->end) { lexer_error(l,"Unterminated escape");return; }
                v=(unsigned char)*l->at++;
                if (v=='n') v='\n';
                else if (v=='r') v='\r';
                else if (v=='t') v='\t';
                else if (isdigit(v)) {
                    int number=v-'0';
                    for (int i=0;i<2 && l->at<l->end && isdigit((unsigned char)*l->at);i++) number=number*10+(*l->at++-'0');
                    v=(unsigned char)number;
                }
            }
            if (n+1>=sizeof l->text) { lexer_error(l,"String too long");return; }
            l->text[n++]=(char)v;
        }
        if (l->at>=l->end) { lexer_error(l,"Unterminated string");return; }
        l->at++;l->text[n]=0;l->token=TOKEN_STRING;return;
    }
    if (isdigit(c) || c=='.' || (c=='-' && l->at<l->end && (isdigit((unsigned char)*l->at) || *l->at=='.'))) {
        char *after;errno=0;l->number=strtod(l->at-1,&after);
        if (after==l->at-1 || errno || !isfinite(l->number) || fabs(l->number)>FLT_MAX) { lexer_error(l,"Invalid numeric data");return; }
        l->at=after;l->token=TOKEN_NUMBER;return;
    }
    l->token=c;l->text[0]=(char)c;l->text[1]=0;
}

static bool take(Lexer *l,int token)
{
    if (l->token!=token) { lexer_error(l,"Unexpected data syntax");return false; }
    next(l);return !l->failed;
}

static bool word(Lexer *l,const char *expected)
{
    if (l->token!=TOKEN_WORD || strcmp(l->text,expected)) { lexer_error(l,"Unsupported executable syntax");return false; }
    next(l);return !l->failed;
}

static bool string_value(Lexer *l,char *out,size_t size)
{
    if (l->token!=TOKEN_STRING || strlen(l->text)>=size) { lexer_error(l,"Expected bounded string");return false; }
    strcpy(out,l->text);next(l);return !l->failed;
}

static bool number_value(Lexer *l,float *value)
{
    if (l->token!=TOKEN_NUMBER) { lexer_error(l,"Expected numeric constant");return false; }
    *value=(float)l->number;next(l);return !l->failed;
}

/* Retail Calibri metrics contain offsets such as 256 + 24 and 512-24.
 * Accept only bounded sums/differences of literals, never script evaluation. */
static bool metric_value(Lexer *l,float *value)
{
    if (!number_value(l,value)) return false;
    while (l->token=='+' || l->token=='-' || (l->token==TOKEN_NUMBER && l->number<0)) {
        int sign=1;
        if (l->token=='+' || l->token=='-') { sign=l->token=='-'?-1:1;next(l); }
        float term;if (!number_value(l,&term)) return false;
        double result=(double)*value+sign*(double)term;
        if (!isfinite(result) || result<INT_MIN || result>INT_MAX) { lexer_error(l,"Glyph metric exceeds integer range");return false; }
        *value=(float)result;
    }
    return true;
}

static bool skip_data(Lexer *l,int depth)
{
    if (depth>32) { lexer_error(l,"Data nesting exceeds limit");return false; }
    if (l->token=='{') {
        next(l);
        while (l->token!='}' && !l->failed) {
            if (l->token==TOKEN_END) { lexer_error(l,"Truncated data table");return false; }
            if (l->token==',' || l->token==';' || l->token=='=' || l->token=='[' || l->token==']') { next(l);continue; }
            if (!skip_data(l,depth+1)) return false;
        }
        return take(l,'}');
    }
    if (l->token==TOKEN_NUMBER || l->token==TOKEN_STRING || l->token==TOKEN_WORD) { next(l);return !l->failed; }
    lexer_error(l,"Expected constant data");return false;
}

static bool frame_record(Lexer *l,SpriteFrame *out)
{
    SpriteFrame f={0};unsigned seen=0;
    static const char *names[]={"dx","dy","u","v","cu","cv","w","h","body_dx","body_dy","body_w","body_h"};
    float *values[]={&f.dx,&f.dy,&f.u,&f.v,&f.cu,&f.cv,&f.w,&f.h,&f.body_dx,&f.body_dy,&f.body_w,&f.body_h};
    if (!take(l,'{')) return false;
    while (l->token!='}' && !l->failed) {
        if (l->token!=TOKEN_WORD) { lexer_error(l,"Expected frame field");return false; }
        int index=-1;
        for (int i=0;i<12;i++) if (!strcmp(l->text,names[i])) index=i;
        if (index<0) { lexer_error(l,"Unknown frame field");return false; }
        next(l);
        if (!take(l,'=') || !number_value(l,values[index])) return false;
        seen|=1u<<index;
        if (l->token==',' || l->token==';') next(l);
    }
    if (!take(l,'}')) return false;
    if ((seen&255)!=255 || f.w<=0 || f.h<=0 || f.cu<=0 || f.cv<=0 || f.u<0 || f.v<0 || f.cu>1 || f.cv>1 ||
        f.u+f.cu>1.00001f || f.v+f.cv>1.00001f) { lexer_error(l,"Incomplete or invalid sprite frame");return false; }
    if ((seen&0xf00) && ((seen&0xf00)!=0xf00 || f.body_w<=0 || f.body_h<=0 ||
        f.body_dx>f.dx+.001f || f.body_dy>f.dy+.001f ||
        f.body_w-f.body_dx>f.w-f.dx+.001f || f.body_h-f.body_dy>f.h-f.dy+.001f)) {
        lexer_error(l,"Incomplete or invalid sprite body bounds");return false;
    }
    *out=f;return true;
}

static bool frame_table(Lexer *l,Sprite *sprite)
{
    if (!sprite) { lexer_error(l,"Frames declared without an animation");return false; }
    if (!take(l,'{')) return false;
    while (l->token!='}' && !l->failed) {
        if (l->token==',' || l->token==';') { next(l);continue; }
        SpriteFrame f;
        if (!frame_record(l,&f)) return false;
        if (!catalog_add_frame(sprite,f)) { l->failed=true;return false; }
    }
    return take(l,'}');
}

static bool glyph_table(Lexer *l,Font *font)
{
    if (!font) { lexer_error(l,"Glyphs declared without font");return false; }
    if (!take(l,'{')) return false;
    while (l->token!='}' && !l->failed) {
        if (l->token==',' || l->token==';') { next(l);continue; }
        if (!take(l,'{')) return false;
        Glyph g={0};bool has_code=false;
        while (l->token!='}' && !l->failed) {
            char field[32];
            if (l->token!=TOKEN_WORD || strlen(l->text)>=sizeof field) { lexer_error(l,"Invalid font field");return false; }
            strcpy(field,l->text);next(l);
            if (!take(l,'=')) return false;
            if (!strcmp(field,"ch")) {
                char ch[32];if (!string_value(l,ch,sizeof ch)) return false;
                const char *p=ch;g.code=utf8_next(&p);has_code=true;
            } else {
                float value;if (!metric_value(l,&value)) return false;
                if ((double)value<INT_MIN || (double)value>INT_MAX) { lexer_error(l,"Glyph metric exceeds integer range");return false; }
                if (!strcmp(field,"x")) g.x=(int)value;
                else if (!strcmp(field,"y")) g.y=(int)value;
                else if (!strcmp(field,"w")) g.w=(int)value;
                else if (!strcmp(field,"h")) g.h=(int)value;
                else if (!strcmp(field,"bX")) g.bx=(int)value;
                else if (!strcmp(field,"bY")) g.by=(int)value;
                else if (!strcmp(field,"aX")) g.advance=(int)value;
                else if (strcmp(field,"aY")) { lexer_error(l,"Unknown glyph metric");return false; }
            }
            if (l->token==',' || l->token==';') next(l);
        }
        if (!take(l,'}') || !has_code) return false;
        Glyph *glyphs=realloc(font->glyphs,(size_t)(font->count+1)*sizeof g);
        if (!glyphs) { lexer_error(l,"Out of memory");return false; }
        font->glyphs=glyphs;font->glyphs[font->count++]=g;
    }
    return take(l,'}');
}

bool retail_declarations_parse(const char *source,size_t size,const char *path,bool font_file,const DeclarationScope *scope)
{
    Lexer l={.source=source,.at=source,.end=source+size,.path=path,.line=1};next(&l);
    Sprite *current=NULL;Font *font=NULL;bool ok=true;
    while (l.token!=TOKEN_END && !l.failed && ok) {
        if (l.token==';') { next(&l);continue; }
        if (l.token!=TOKEN_WORD) { lexer_error(&l,"Expected data declaration");break; }
        char symbol[128];
        if (strlen(l.text)>=sizeof symbol) { lexer_error(&l,"Name too long");break; }
        strcpy(symbol,l.text);next(&l);
        if (l.token=='=') {
            next(&l);
            if (l.token==TOKEN_WORD && !strcmp(l.text,"nil")) {
                next(&l);
                if (!strcmp(symbol,"texinfo")) current=NULL;
                else if (!strcmp(symbol,"font")) font=NULL;
                continue;
            }
            if (!strcmp(symbol,"texinfo") && !font_file) {
                char name[128];
                ok=word(&l,"createTextureInfoEx") && take(&l,'(') && string_value(&l,name,sizeof name) && take(&l,')');
                if (ok) ok=(current=catalog_new_sprite(name,scope->source))!=NULL;
            } else if (!strcmp(symbol,"frames") && !font_file) ok=frame_table(&l,current);
            else if (!strcmp(symbol,"font") && font_file) {
                char name[128];
                ok=word(&l,"createFont") && take(&l,'(') && string_value(&l,name,sizeof name) && take(&l,')');
                if (ok) ok=(font=catalog_new_font(name))!=NULL;
            } else if (!strcmp(symbol,"chars") && font_file) ok=glyph_table(&l,font);
            else if (!strcmp(symbol,"bounds") || !strcmp(symbol,"points")) ok=skip_data(&l,0);
            else { lexer_error(&l,"Unsupported assignment; scripts are never executed");break; }
        } else if (!strcmp(symbol,"addTextureInfoExNodes") && !font_file) {
            char texture[512];float fps;
            ok=current && take(&l,'(') && word(&l,"texinfo") && take(&l,',') && string_value(&l,texture,sizeof texture) &&
                take(&l,',') && word(&l,"frames") && take(&l,',') && number_value(&l,&fps) && take(&l,')');
            if (ok && (fps<0 || fps>1000)) { lexer_error(&l,"Invalid animation frame rate");ok=false; }
            if (ok) {
                char relative[640],resolved[PATH_CAPACITY];
                snprintf(relative,sizeof relative,"texture/lazy/%s.png",texture);
                if (!asset_resolve(scope,relative,resolved,sizeof resolved)) {
                    error_set("%s:%d: missing sprite texture %s",path,l.line,relative);ok=false;l.failed=true;
                } else ok=catalog_set_texture(current,resolved);
                current->fps=fps;
            }
        } else if ((!strcmp(symbol,"addBoundInfoExNodes") || !strcmp(symbol,"addPointsInfoExNodes")) && !font_file) {
            ok=take(&l,'(') && word(&l,"texinfo") && take(&l,',') &&
                word(&l,!strcmp(symbol,"addBoundInfoExNodes")?"bounds":"points") && take(&l,')');
        } else if (!strcmp(symbol,"initFont") && font_file) {
            char family[128],style[128];float value;
            ok=font && take(&l,'(') && word(&l,"font") && take(&l,',') && string_value(&l,family,sizeof family) && take(&l,',') &&
                string_value(&l,style,sizeof style) && take(&l,',') && number_value(&l,&value) && take(&l,')');
            if (ok) font->size=(int)value;
        } else if (!strcmp(symbol,"setFontTexture") && font_file) {
            char texture[512];float width,height;
            ok=font && take(&l,'(') && word(&l,"font") && take(&l,',') && string_value(&l,texture,sizeof texture) && take(&l,',') &&
                number_value(&l,&width) && take(&l,',') && number_value(&l,&height) && take(&l,')');
            if (ok) {
                char relative[640];snprintf(relative,sizeof relative,"font/en/%s.png",texture);
                if (!asset_resolve(scope,relative,font->texture,sizeof font->texture)) {
                    error_set("%s: missing font texture %s",path,relative);ok=false;l.failed=true;
                }
            }
        } else if (!strcmp(symbol,"setFontTransform") && font_file) {
            float x,y;
            ok=font && take(&l,'(') && word(&l,"font") && take(&l,',') && number_value(&l,&x) && take(&l,',') && number_value(&l,&y) && take(&l,')');
        } else if (!strcmp(symbol,"fillFont") && font_file) {
            ok=font && take(&l,'(') && word(&l,"font") && take(&l,',') && word(&l,"chars") && take(&l,')');
        } else { lexer_error(&l,"Unsupported function; scripts are never executed");break; }
    }
    if (!ok && !l.failed && !last_error[0]) lexer_error(&l,"Incomplete resource declaration");
    return ok && !l.failed;
}

bool retail_declarations_selftest(void)
{
    static const char *rejected[]={
        "os.execute('must not run')",
        "require('anything')",
        "texinfo = dofile('anything')",
        "bounds = { nested = function() end }",
        "bounds = { 1e100 }",
        "bounds = { 1e999 }",
        "bounds = { 0, 1 ",
        "--[[",
        "--[[missing close",
        "texinfo = createTextureInfoEx('unterminated)",
    };
    DeclarationScope scope={NULL,-1000};
    for (size_t i=0;i<sizeof rejected/sizeof *rejected;i++)
        if (retail_declarations_parse(rejected[i],strlen(rejected[i]),"rejection self-test",false,&scope))
            return error_set("Declaration parser accepted unsupported input: %s",rejected[i]);
    const char *valid="--[[constant bounds]]\nbounds = { { x=1.25, y=-2 }, {3,4} }";
    if (!retail_declarations_parse(valid,strlen(valid),"acceptance self-test",false,&scope)) return false;
    const char *invalid_frame="{dx=0,dy=0,u=1e20,v=0,cu=.5,cv=.5,w=32,h=32}";
    Lexer lexer={.source=invalid_frame,.at=invalid_frame,.end=invalid_frame+strlen(invalid_frame),.path="UV self-test",.line=1};
    SpriteFrame f;next(&lexer);
    if (frame_record(&lexer,&f)) return error_set("Declaration parser accepted invalid atlas UV bounds");
    static const char *body_frames[]={
        "{dx=20,dy=12,u=0,v=0,cu=.5,cv=.5,w=48,h=48,body_dx=16,body_dy=8,body_w=32,body_h=32}",
        "{dx=20,dy=12,u=0,v=0,cu=.5,cv=.5,w=48,h=48,body_w=32}",
        "{dx=20,dy=12,u=0,v=0,cu=.5,cv=.5,w=48,h=48,body_dx=21,body_dy=8,body_w=32,body_h=32}",
        "{dx=20,dy=12,u=0,v=0,cu=.5,cv=.5,w=48,h=48,body_dx=16,body_dy=8,body_w=64,body_h=32}",
    };
    for (size_t i=0;i<sizeof body_frames/sizeof *body_frames;i++) {
        const char *text=body_frames[i];
        Lexer l={.source=text,.at=text,.end=text+strlen(text),.path="body self-test",.line=1};next(&l);
        bool accepted=frame_record(&l,&f);
        if (accepted!=(i==0)) return error_set("Invalid sprite body bounds handling (case %zu)",i);
        if (accepted) {
            SpriteFrame body=sprite_body_frame(f);
            if (body.dx!=16 || body.dy!=8 || body.w!=32 || body.h!=32) return error_set("Sprite body bounds not applied");
        }
    }
    error_clear();return true;
}
