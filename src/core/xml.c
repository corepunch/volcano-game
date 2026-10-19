#include "xml.h"
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { XML_MAX_DEPTH=64 };
typedef struct { XmlDoc *doc;const char *at,*end;int line; } Parser;

static bool parse_error(Parser *p,const char *message)
{
    return error_set("%s:%d: %s",p->doc->source,p->line,message);
}

static bool starts(Parser *p,const char *text)
{
    size_t n=strlen(text);return (size_t)(p->end-p->at)>=n && !memcmp(p->at,text,n);
}

static void advance(Parser *p,size_t n)
{
    for (size_t i=0;i<n && p->at<p->end;i++) { if (*p->at=='\n') p->line++;p->at++; }
}

static bool is_space(char c) { return c==' ' || c=='\t' || c=='\n' || c=='\r'; }
static void skip_space(Parser *p) { while (p->at<p->end && is_space(*p->at)) advance(p,1); }

static bool name_start(unsigned char c) { return c=='_' || c==':' || (c>='a' && c<='z') || (c>='A' && c<='Z') || c>=0x80; }
static bool name_char(unsigned char c) { return name_start(c) || c=='-' || c=='.' || (c>='0' && c<='9'); }

static bool parse_name(Parser *p,const char **out)
{
    const char *start=p->at;
    if (p->at>=p->end || !name_start((unsigned char)*p->at)) return parse_error(p,"expected a name");
    while (p->at<p->end && name_char((unsigned char)*p->at)) p->at++;
    size_t n=(size_t)(p->at-start);char *name=arena_alloc(&p->doc->arena,n+1);
    if (!name) return false;
    memcpy(name,start,n);*out=name;return true;
}

static bool skip_until(Parser *p,const char *terminator,const char *message)
{
    while (p->at<p->end && !starts(p,terminator)) advance(p,1);
    if (p->at>=p->end) return parse_error(p,message);
    advance(p,strlen(terminator));return true;
}

/* Comments and processing instructions; DOCTYPE is refused outright. */
static bool skip_misc(Parser *p,bool *skipped)
{
    *skipped=true;
    if (starts(p,"<!--")) {
        advance(p,4);
        while (p->at<p->end && !starts(p,"-->")) {
            if (starts(p,"--")) return parse_error(p,"\"--\" inside a comment");
            advance(p,1);
        }
        if (p->at>=p->end) return parse_error(p,"unterminated comment");
        advance(p,3);return true;
    }
    if (starts(p,"<!DOCTYPE") || starts(p,"<!ENTITY")) return parse_error(p,"DOCTYPE and entity declarations are not supported");
    if (starts(p,"<?")) return skip_until(p,"?>","unterminated processing instruction");
    *skipped=false;return true;
}

static size_t utf8_encode(char *out,uint32_t c)
{
    if (c<0x80) { out[0]=(char)c;return 1; }
    if (c<0x800) { out[0]=(char)(0xC0|c>>6);out[1]=(char)(0x80|(c&63));return 2; }
    if (c<0x10000) { out[0]=(char)(0xE0|c>>12);out[1]=(char)(0x80|(c>>6&63));out[2]=(char)(0x80|(c&63));return 3; }
    out[0]=(char)(0xF0|c>>18);out[1]=(char)(0x80|(c>>12&63));out[2]=(char)(0x80|(c>>6&63));out[3]=(char)(0x80|(c&63));return 4;
}

/* Decodes character data up to STOP (a quote, or '<' for text) into BUFFER. */
static bool decode_text(Parser *p,char stop,char *buffer,size_t *length)
{
    size_t n=*length;
    while (p->at<p->end && *p->at!=stop) {
        char c=*p->at;
        if (c=='<') return parse_error(p,"'<' in attribute value");
        if (c=='&') {
            static const struct { const char *name;char value; } entities[]={
                {"&lt;",'<'},{"&gt;",'>'},{"&amp;",'&'},{"&quot;",'"'},{"&apos;",'\''}};
            bool known=false;
            for (size_t i=0;i<sizeof entities/sizeof *entities && !known;i++) if (starts(p,entities[i].name)) {
                buffer[n++]=entities[i].value;advance(p,strlen(entities[i].name));known=true;
            }
            if (known) continue;
            if (!starts(p,"&#")) return parse_error(p,"unknown entity (only &lt; &gt; &amp; &quot; &apos; and &#N; are allowed)");
            advance(p,2);bool hex=p->at<p->end && *p->at=='x';if (hex) advance(p,1);
            uint32_t code=0;int digits=0;
            while (p->at<p->end && *p->at!=';') {
                char d=*p->at;int v=d>='0' && d<='9'?d-'0':hex && d>='a' && d<='f'?d-'a'+10:hex && d>='A' && d<='F'?d-'A'+10:-1;
                if (v<0 || ++digits>8) return parse_error(p,"invalid character reference");
                code=code*(hex?16:10)+(uint32_t)v;advance(p,1);
            }
            if (p->at>=p->end || !digits || !code || code>0x10FFFF || (code>=0xD800 && code<=0xDFFF))
                return parse_error(p,"invalid character reference");
            advance(p,1);n+=utf8_encode(buffer+n,code);continue;
        }
        if (stop=='<' && starts(p,"]]>")) return parse_error(p,"\"]]>\" in character data");
        buffer[n++]=c;advance(p,1);
    }
    *length=n;return true;
}

static bool parse_element(Parser *p,XmlNode *node,int depth);

static bool append_child(Parser *p,XmlNode **children,int *count,int *capacity,XmlNode **out)
{
    if (*count==*capacity) {
        int grown_capacity=*capacity?*capacity*2:8;
        XmlNode *grown=realloc(*children,(size_t)grown_capacity*sizeof *grown);
        if (!grown) return parse_error(p,"out of memory");
        *children=grown;*capacity=grown_capacity;
    }
    *out=&(*children)[(*count)++];return true;
}

static bool parse_element(Parser *p,XmlNode *node,int depth)
{
    if (depth>XML_MAX_DEPTH) return parse_error(p,"nesting too deep");
    memset(node,0,sizeof *node);node->line=p->line;node->text="";
    advance(p,1);
    if (!parse_name(p,&node->name)) return false;
    const char *keys[64],*values[64];int count=0;
    for (;;) {
        bool spaced=p->at<p->end && is_space(*p->at);skip_space(p);
        if (p->at>=p->end) return parse_error(p,"unterminated start tag");
        if (*p->at=='/' || *p->at=='>') break;
        if (!spaced) return parse_error(p,"expected whitespace before attribute");
        if (count==64) return parse_error(p,"too many attributes");
        if (!parse_name(p,&keys[count])) return false;
        for (int i=0;i<count;i++) if (!strcmp(keys[i],keys[count])) return parse_error(p,"duplicate attribute");
        skip_space(p);
        if (p->at>=p->end || *p->at!='=') return parse_error(p,"expected '=' after attribute name");
        advance(p,1);skip_space(p);
        if (p->at>=p->end || (*p->at!='"' && *p->at!='\'')) return parse_error(p,"attribute values must be quoted");
        char quote=*p->at;advance(p,1);
        const char *start=p->at;while (p->at<p->end && *p->at!=quote) p->at++;
        if (p->at>=p->end) return parse_error(p,"unterminated attribute value");
        char *value=arena_alloc(&p->doc->arena,(size_t)(p->at-start)+1);size_t n=0;
        if (!value) return false;
        p->at=start;
        if (!decode_text(p,quote,value,&n)) return false;
        value[n]=0;values[count++]=value;advance(p,1);
    }
    node->attribute_count=count;
    node->keys=arena_alloc(&p->doc->arena,(size_t)(count?count:1)*sizeof *keys);
    node->values=arena_alloc(&p->doc->arena,(size_t)(count?count:1)*sizeof *values);
    if (!node->keys || !node->values) return false;
    memcpy(node->keys,keys,(size_t)count*sizeof *keys);memcpy(node->values,values,(size_t)count*sizeof *values);
    if (*p->at=='/') {
        advance(p,1);
        if (p->at>=p->end || *p->at!='>') return parse_error(p,"expected '>'");
        advance(p,1);return true;
    }
    advance(p,1);
    XmlNode *children=NULL;int child_count=0,capacity=0;
    char *text=NULL;size_t text_length=0,text_capacity=0;bool ok=true;
    while (ok) {
        if (p->at>=p->end) { ok=parse_error(p,"unterminated element");break; }
        if (starts(p,"</")) {
            advance(p,2);const char *name;
            if (!(ok=parse_name(p,&name))) break;
            if (strcmp(name,node->name)) {
                char message[200];snprintf(message,sizeof message,"</%.60s> closes <%.60s>",name,node->name);
                ok=parse_error(p,message);break;
            }
            skip_space(p);
            if (p->at>=p->end || *p->at!='>') { ok=parse_error(p,"expected '>'");break; }
            advance(p,1);break;
        }
        bool skipped;
        if (!(ok=skip_misc(p,&skipped))) break;
        if (skipped) continue;
        if (starts(p,"<![CDATA[")) {
            advance(p,9);const char *start=p->at;
            while (p->at<p->end && !starts(p,"]]>")) advance(p,1);
            if (p->at>=p->end) { ok=parse_error(p,"unterminated CDATA");break; }
            size_t n=(size_t)(p->at-start);
            if (text_length+n+1>text_capacity) {
                text_capacity=(text_length+n+1)*2;char *grown=realloc(text,text_capacity);
                if (!grown) { ok=parse_error(p,"out of memory");break; }
                text=grown;
            }
            memcpy(text+text_length,start,n);text_length+=n;advance(p,3);continue;
        }
        if (*p->at=='<') {
            XmlNode *child=NULL;
            if (!(ok=append_child(p,&children,&child_count,&capacity,&child))) break;
            ok=parse_element(p,child,depth+1);continue;
        }
        const char *start=p->at;while (p->at<p->end && *p->at!='<') p->at++;
        size_t raw=(size_t)(p->at-start);p->at=start;
        if (text_length+raw+1>text_capacity) {
            text_capacity=(text_length+raw+1)*2;char *grown=realloc(text,text_capacity);
            if (!grown) { ok=parse_error(p,"out of memory");break; }
            text=grown;
        }
        ok=decode_text(p,'<',text,&text_length);
    }
    if (ok && text) {
        size_t first=0,last=text_length;
        while (first<last && is_space(text[first])) first++;
        while (last>first && is_space(text[last-1])) last--;
        if (last>first && child_count) {
            p->line=node->line;ok=parse_error(p,"mixed text and child elements are not supported");
        } else if (last>first) {
            char *trimmed=arena_alloc(&p->doc->arena,last-first+1);
            if ((ok=trimmed!=NULL)) { memcpy(trimmed,text+first,last-first);node->text=trimmed; }
        }
    }
    if (ok && child_count) {
        node->children=arena_alloc(&p->doc->arena,(size_t)child_count*sizeof *children);
        if ((ok=node->children!=NULL)) { memcpy(node->children,children,(size_t)child_count*sizeof *children);node->child_count=child_count; }
    }
    free(children);free(text);return ok;
}

bool xml_parse(XmlDoc *doc,const char *text,size_t length,const char *source)
{
    memset(doc,0,sizeof *doc);
    if (!text_copy(doc->source,sizeof doc->source,source)) text_copy(doc->source,sizeof doc->source,"(source)");
    if (memchr(text,0,length)) return error_set("%s: NUL byte in XML",doc->source);
    Parser p={doc,text,text+length,1};
    if (length>=3 && !memcmp(text,"\xEF\xBB\xBF",3)) p.at+=3;
    bool ok=true,skipped=true;
    while (ok && skipped) { skip_space(&p);ok=skip_misc(&p,&skipped); }
    if (ok && (p.at>=p.end || *p.at!='<')) ok=parse_error(&p,"expected a root element");
    if (ok) ok=(doc->root=arena_alloc(&doc->arena,sizeof *doc->root))!=NULL && parse_element(&p,doc->root,0);
    skipped=true;
    while (ok && skipped) { skip_space(&p);ok=skip_misc(&p,&skipped); }
    if (ok && p.at!=p.end) ok=parse_error(&p,"content after the root element");
    if (!ok) xml_free(doc);
    return ok;
}

bool xml_load(XmlDoc *doc,const char *path)
{
    size_t length;char *text=file_read(path,32u<<20,&length);
    if (!text) { memset(doc,0,sizeof *doc);return false; }
    bool ok=xml_parse(doc,text,length,path);free(text);return ok;
}

void xml_free(XmlDoc *doc) { arena_free(&doc->arena);doc->root=NULL; }

const char *xml_attr(const XmlNode *node,const char *key)
{
    for (int i=0;node && i<node->attribute_count;i++) if (!strcmp(node->keys[i],key)) return node->values[i];
    return NULL;
}

const XmlNode *xml_next(const XmlNode *node,const char *name,const XmlNode *after)
{
    int i=after?(int)(after-node->children)+1:0;
    for (;i<node->child_count;i++) if (!name || !strcmp(node->children[i].name,name)) return &node->children[i];
    return NULL;
}

int xml_count(const XmlNode *node,const char *name)
{
    int count=0;
    XML_EACH(child,node,name) count++;
    return count;
}

bool xml_fail(const XmlDoc *doc,const XmlNode *at,const char *format,...)
{
    char message[768];va_list args;va_start(args,format);vsnprintf(message,sizeof message,format,args);va_end(args);
    return error_set("%s:%d: <%s> %s",doc->source,at?at->line:0,at?at->name:"?",message);
}

bool xml_only(const XmlDoc *doc,const XmlNode *node,const char *const *attributes,const char *const *children)
{
    for (int i=0;i<node->attribute_count;i++) {
        bool known=false;
        for (const char *const *a=attributes;a && *a && !known;a++) known=!strcmp(node->keys[i],*a);
        if (!known) return xml_fail(doc,node,"unknown attribute \"%s\"",node->keys[i]);
    }
    for (int i=0;i<node->child_count;i++) {
        bool known=false;
        for (const char *const *c=children;c && *c && !known;c++) known=!strcmp(node->children[i].name,*c);
        if (!known) return xml_fail(doc,&node->children[i],"is not allowed inside <%s>",node->name);
    }
    if (*node->text && !node->child_count && children && *children)
        return xml_fail(doc,node,"has unexpected text");
    return true;
}

bool xml_require(const XmlDoc *doc,const XmlNode *node,const char *key,const char **out)
{
    if (!(*out=xml_attr(node,key)) || !**out) return xml_fail(doc,node,"requires attribute \"%s\"",key);
    return true;
}

static bool parse_double(const char *text,double *out)
{
    char *end;errno=0;double value=strtod(text,&end);
    if (end==text || *end || errno==ERANGE || !isfinite(value)) return false;
    *out=value;return true;
}

bool xml_float(const XmlDoc *doc,const XmlNode *node,const char *key,float fallback,float min,float max,float *out)
{
    const char *text=xml_attr(node,key);double value;
    if (!text) { *out=fallback;return true; }
    if (!parse_double(text,&value)) return xml_fail(doc,node,"%s=\"%s\" is not a number",key,text);
    if (value<min || value>max) return xml_fail(doc,node,"%s=%g is outside %g..%g",key,value,min,max);
    *out=(float)value;return true;
}

bool xml_int(const XmlDoc *doc,const XmlNode *node,const char *key,int fallback,int min,int max,int *out)
{
    const char *text=xml_attr(node,key);double value;
    if (!text) { *out=fallback;return true; }
    if (!parse_double(text,&value) || value!=floor(value)) return xml_fail(doc,node,"%s=\"%s\" is not an integer",key,text);
    if (value<min || value>max) return xml_fail(doc,node,"%s=%g is outside %d..%d",key,value,min,max);
    *out=(int)value;return true;
}

bool xml_uint(const XmlDoc *doc,const XmlNode *node,const char *key,uint32_t fallback,uint32_t *out)
{
    const char *text=xml_attr(node,key);double value;
    if (!text) { *out=fallback;return true; }
    if (!parse_double(text,&value) || value!=floor(value) || value<0 || value>4294967295.0)
        return xml_fail(doc,node,"%s=\"%s\" is not an unsigned 32-bit integer",key,text);
    *out=(uint32_t)value;return true;
}

bool xml_bool(const XmlDoc *doc,const XmlNode *node,const char *key,bool fallback,bool *out)
{
    const char *text=xml_attr(node,key);
    if (!text) { *out=fallback;return true; }
    if (!strcmp(text,"true")) *out=true;
    else if (!strcmp(text,"false")) *out=false;
    else return xml_fail(doc,node,"%s=\"%s\": expected true or false",key,text);
    return true;
}

bool xml_string(const XmlDoc *doc,const XmlNode *node,const char *key,const char *fallback,const char **out)
{
    (void)doc;const char *text=xml_attr(node,key);*out=text?text:fallback;return true;
}

bool xml_floats(const XmlDoc *doc,const XmlNode *node,const char *key,float *out,int count,bool required)
{
    const char *text=xml_attr(node,key);
    if (!text) return required?xml_fail(doc,node,"requires attribute \"%s\"",key):true;
    for (int i=0;i<count;i++) {
        char *end;errno=0;double value=strtod(text,&end);
        bool separator=i<count-1?*end==',':*end==0;
        if (end==text || errno==ERANGE || !isfinite(value) || !separator)
            return xml_fail(doc,node,"%s: expected %d comma-separated numbers",key,count);
        out[i]=(float)value;text=end+1;
    }
    return true;
}

bool xml_points(const XmlDoc *doc,const XmlNode *node,Arena *arena,Vec2 **points,int *count)
{
    int capacity=0;const char *at=node->text;
    for (const char *s=at;*s;s++) if (*s==',') capacity++;
    *points=arena_alloc(arena,(size_t)(capacity?capacity:1)*sizeof **points);*count=0;
    if (!*points) return false;
    while (*at) {
        while (is_space(*at)) at++;
        if (!*at) break;
        char *end;errno=0;double x=strtod(at,&end);
        if (end==at || *end!=',' || errno==ERANGE || !isfinite(x)) return xml_fail(doc,node,"point %d: expected x,y",*count+1);
        at=end+1;double y=strtod(at,&end);
        if (end==at || (*end && !is_space(*end)) || errno==ERANGE || !isfinite(y)) return xml_fail(doc,node,"point %d: expected x,y",*count+1);
        (*points)[(*count)++]=vec2((float)x,(float)y);at=end;
    }
    return true;
}
