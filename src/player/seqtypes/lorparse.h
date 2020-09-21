#ifndef LIBREORAMA_LORPARSE_H
#define LIBREORAMA_LORPARSE_H

#include <libxml/tree.h>

xmlNode *xml_find_node_next(const xmlNode *root,
                            const char *name);

xmlNode *xml_find_node_child(const xmlNode *parent,
                             const char *name);

int xml_get_property(const xmlNode *node,
                     const char *key,
                     char **out);

long xml_get_propertyl(const xmlNode *node,
                       const char *key);

int xml_is_named_node(const xmlNode *node,
                      const char *key);

#endif //LIBREORAMA_LORPARSE_H
