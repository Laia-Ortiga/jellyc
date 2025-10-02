#pragma once

#include "ast.h"
#include "fwd.h"
#include "hash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    TIRCAT_ERROR,
    TIRCAT_OTHER,
    TIRCAT_TYPE,
    TIRCAT_VALUE,
    TIRCAT_MACRO,
    TIRCAT_MODULE,
} TirCategory;

typedef enum {
    VALUE_INVALID,
    VALUE_TEMPORARY,
    VALUE_PLACE,
    VALUE_SLICE,
} ValueCategory;

typedef struct {
    int32_t id;
} TirId;

static TirId const error_term = {0};
#define ptype(type) ((TirId) {TYPE_##type})

typedef struct {
    int32_t capacity;
    int32_t count;
    TirId *ptr;
} TermSet;

typedef struct {
    int32_t a;
    int32_t b;
    int32_t c;
    int32_t d;
} TermData;

typedef struct {
    SumVec(TermData) terms;
    Vec(int32_t) extra;
    TermSet set;
} TermList;

typedef struct {
    AstId ast_id;
    union {
        int32_t field_index;
        TirId enum_value;
    };
} TypeScopeSymbol;

typedef struct {
    StringBuffer strtab;
    TermList terms;
    TirId main;
    Vec(TirId) structs;
    Vec(TirId) extern_vars;
    Vec(TirId) extern_functions;
    Vec(TirId) functions;
    Vec(TypeScopeSymbol) type_scope_symbols;
    Vec(HashTable) type_scopes;
} Tir;

typedef struct {
    int32_t body_first;
    int32_t body_length;
    Tir deps;
    int32_t local_count;
} LocalTir;

typedef struct {
    Tir *global;
    Tir *thread;
} TirContext;

#include "tir-types.h"

// Constructors

TirId new_tir(TirContext c, TirTag tag, TermData data);

TirId new_array_type(TirContext c, TirArrayType t);
TirId new_array_length_type(TirContext c, int64_t length);
TirId new_ptr_type(TirContext c, TirId elem);
TirId new_mut_ptr_type(TirContext c, TirId elem);
TirId new_slice_type(TirContext c, TirId elem);
TirId new_mut_slice_type(TirContext c, TirId elem);
TirId new_function_type(TirContext c, TirFunctionType t);
TirId new_struct_type(TirContext c, Target target, TirStructType t);
TirId new_affine_type(TirContext c, TirId elem);
TirId new_tagged_type(TirContext c, TirTaggedType t);

// Other

TirTag get_term_tag(TirContext c, TirId type);
TermData const *get_term_data(TirContext c, TirId term);
int32_t get_term_extra(Tir *c, int32_t index);
TirCategory get_term_category(TirContext c, TirId term);

TirId get_value_type(TirContext c, TirId value);
ValueCategory get_value_category(TirContext c, TirId value);
bool is_value_mutable(TirContext c, TirId value);

TirId get_function_type_param(TirContext c, TirId type, int32_t index);
TirId get_struct_type_field(TirContext c, TirId type, int32_t index);
TirId get_tagged_type_arg(TirContext c, TirId type, int32_t index);
TirGeneric as_generic_term(TirContext c, TirId term);

char const *tir_get_str(TirContext c, int32_t s);
int32_t tir_push_str(TirContext c, String s);
int32_t tir_push_cstr(TirContext c, String s);

static inline Tir *tir_writer(TirContext c) {
    return c.thread ? c.thread : c.global;
}

static inline Tir *tir_get_storage(TirContext c, TirId term) {
    if (term.id - TERM_COUNT < c.global->terms.terms.len) {
        return c.global;
    }
    return c.thread;
}
