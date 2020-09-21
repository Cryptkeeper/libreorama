#ifndef LIBREORAMA_LOREDIT_H
#define LIBREORAMA_LOREDIT_H

#include "../sequence.h"

int loredit_sequence_load(const char *sequence_file,
                          char **audio_file_hint,
                          struct sequence_t *sequence);

#endif //LIBREORAMA_LOREDIT_H
