#ifndef LIBREORAMA_LOREFFECT_H
#define LIBREORAMA_LOREFFECT_H

#include <libxml/tree.h>

#include "../../lorinterface/frame.h"

unsigned char loreffect_brightness(unsigned char effect_intensity);

int loreffect_get_frame(const xmlNode *effect_node,
                        struct frame_t *frame,
                        unsigned long start_cs,
                        unsigned long end_cs);

#endif //LIBREORAMA_LOREFFECT_H
