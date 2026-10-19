#ifndef VOLCANO_XML_H
#define VOLCANO_XML_H
/* Strict XML 1.0 reader for the project's data files (content, missions,
 * sprite sheets). Supported: one root element, attributes, character data,
 * comments, CDATA, an optional <?xml?> declaration, the five predefined
 * entities and numeric character references. Rejected: DOCTYPE and custom
 * entities (no expansion attacks), duplicate attributes, mixed content.
 * Nodes live in the document's arena; helpers report "file:line: message". */
#include "core.h"

typedef struct XmlNode XmlNode;
struct XmlNode {
    const char *name;
    const char **keys,**values;    /* Attributes, in document order. */
    int attribute_count;
    XmlNode *children;
    int child_count,line;
    const char *text;              /* Trimmed character data ("" when none). */
};
typedef struct { Arena arena;XmlNode *root;char source[PATH_CAPACITY]; } XmlDoc;

bool xml_parse(XmlDoc *doc,const char *text,size_t length,const char *source);
bool xml_load(XmlDoc *doc,const char *path);
void xml_free(XmlDoc *doc);

const char *xml_attr(const XmlNode *node,const char *key);
/* Next child named NAME after AFTER (NULL starts at the first). */
const XmlNode *xml_next(const XmlNode *node,const char *name,const XmlNode *after);
int xml_count(const XmlNode *node,const char *name);
#define XML_EACH(child,node,name) for (const XmlNode *child=xml_next((node),(name),NULL);child;child=xml_next((node),(name),child))

bool xml_fail(const XmlDoc *doc,const XmlNode *at,const char *format,...)
#if defined(__GNUC__)
    __attribute__((format(printf,3,4)))
#endif
    ;
/* Rejects attributes outside ATTRIBUTES and child elements outside CHILDREN
 * (both NULL-terminated; NULL means none allowed). */
bool xml_only(const XmlDoc *doc,const XmlNode *node,const char *const *attributes,const char *const *children);
bool xml_require(const XmlDoc *doc,const XmlNode *node,const char *key,const char **out);
/* Attribute readers: absent attributes take the fallback; present ones must validate. */
bool xml_float(const XmlDoc *doc,const XmlNode *node,const char *key,float fallback,float min,float max,float *out);
bool xml_int(const XmlDoc *doc,const XmlNode *node,const char *key,int fallback,int min,int max,int *out);
bool xml_uint(const XmlDoc *doc,const XmlNode *node,const char *key,uint32_t fallback,uint32_t *out);
bool xml_bool(const XmlDoc *doc,const XmlNode *node,const char *key,bool fallback,bool *out);
bool xml_string(const XmlDoc *doc,const XmlNode *node,const char *key,const char *fallback,const char **out);
/* "a,b,..." lists of COUNT finite numbers, e.g. pivot="0.5,0.7". */
bool xml_floats(const XmlDoc *doc,const XmlNode *node,const char *key,float *out,int count,bool required);
/* Whitespace-separated "x,y" points in a node's text, e.g. traffic paths. */
bool xml_points(const XmlDoc *doc,const XmlNode *node,Arena *arena,Vec2 **points,int *count);
#endif
