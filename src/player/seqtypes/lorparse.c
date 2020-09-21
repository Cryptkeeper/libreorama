#include "lorparse.h"

#include <string.h>

#include "../../err/lbr.h"

xmlNode *xml_find_node_next(const xmlNode *root,
                            const char *name) {
    // iterate from the provided element to each next
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = (xmlNode *) root;

    while (cur != NULL) {
        if (xmlStrEqual(cur->name, (const xmlChar *) name)) {
            return cur;
        }

        // move forward to next node
        // this has a search depth of 1 (immediate children only)
        cur = cur->next;
    }

    return NULL;
}

xmlNode *xml_find_node_child(const xmlNode *parent,
                             const char *name) {
    // iterate from the provided element to each child
    // see http://xmlsoft.org/tutorial/ar01s04.html
    xmlNode *cur = parent->children;

    while (cur != NULL) {
        if (xmlStrEqual(cur->name, (const xmlChar *) name)) {
            return cur;
        }

        // move forward to next node
        // this has a search depth of 1 (immediate children only)
        cur = cur->next;
    }

    return NULL;
}

// finds the node property, if any, and copies the string into a char *buffer
// the buffer must be manually freed by the caller function
// returns NULL if the node does not contain the property or malloc fails
int xml_get_property(const xmlNode *node,
                     const char *key,
                     char **out) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);

    if (value == NULL) {
        return 0;
    }

    int return_code = 0;

    // determine the string's length and allocate a copy buffer
    // size for + 1 to ensure null termination
    const size_t str_len = (size_t) xmlStrlen(value);
    char         *copy   = malloc(str_len + 1);

    if (copy == NULL) {
        return_code = LBR_EERRNO;
        goto xml_get_property_free;
    }

    // cast xmlChar back to char
    // this is seemingly dangerous, but apparently expected usage
    // see https://stackoverflow.com/questions/2282424/libxml2-xmlchar-cast-to-char
    strncpy(copy, (const char *) value, str_len);

    // explicitly null terminate the string
    copy[str_len] = 0;

    *out = copy;

    xml_get_property_free:
    // free the allocated xmlChar *string from xmlGetProp
    xmlFree((void *) value);

    return return_code;
}

long xml_get_propertyl(const xmlNode *node,
                       const char *key) {
    xmlChar *value = xmlGetProp(node, (const xmlChar *) key);
    if (value == NULL) {
        return 0;
    }
    const long l = strtol((const char *) value, NULL, 10);
    xmlFree((void *) value);
    return l;
}

int xml_is_named_node(const xmlNode *node,
                      const char *key) {
    return node->type == XML_ELEMENT_NODE && xmlStrEqual(node->name, (const xmlChar *) key);
}