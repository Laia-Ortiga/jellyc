#pragma once

#include "arena.h"
#include "tir.h"

typedef struct TypeMatcher {
    enum {
        TYPE_MATCH_T,
        TYPE_MATCH_BYTE,
        TYPE_MATCH_ARRAY,
        TYPE_MATCH_ANY_POINTER,
        TYPE_MATCH_ANY_SLICE,
        TYPE_MATCH_POINTER,
        TYPE_MATCH_SLICE,
        TYPE_MATCH_MUT_POINTER,
        TYPE_MATCH_MUT_SLICE,
        TYPE_MATCH_TAGGED,
    } match_type;

    int64_t extra;
    struct TypeMatcher *inner;
} TypeMatcher;

int match_types(TirContext c, TirId *results, int32_t count, TirId *types, TypeMatcher *matchers);
int match_type_parameters(TirContext c, TirId *results, TirId param, TirId arg);

typedef struct {
    TirContext c;
    TirId const *args;
    Arena scratch;
    Target target;
} ReplaceTypeInfo;

TirId replace_type_parameters(TirId generic, ReplaceTypeInfo *info);

#define match_ignore 0
#define match_array(I, E, EXTRA) \
    ((TypeMatcher) { \
        .match_type = TYPE_MATCH_ARRAY, \
        .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)} \
    })
#define match_T(EXTRA) \
    ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})
