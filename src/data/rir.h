#pragma once

#include "fwd.h"

#include <stdint.h>

/*
    Role Intermediate Representation

    Removes modules and macros
    Leaves values and types
*/

typedef struct {
    unsigned char *tags;
    int32_t *data;
} Rir;

static inline RirTag get_rir_tag(AstId node, Rir const *rir) {
    return rir->tags[node.private_field_id];
}

static inline int32_t get_rir_data(AstId node, Rir const *rir) {
    return rir->data[node.private_field_id];
}
