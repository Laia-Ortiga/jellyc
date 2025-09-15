#pragma once

#include "arena.h"
#include "enums.h"
#include "ast.h"
#include "hash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int32_t id;
} TirId;

static TirId const null_tir = {0};

// Types

static TirId const type_void = {TYPE_VOID};

#define TYPE(type) static TirId const type_##type = {TYPE_##type};
#include "simple-types"

typedef struct {
    int32_t capacity;
    int32_t count;
    TirId *ptr;
} TermSet;

typedef struct {
    AstId node;
    int32_t a;
    int32_t b;
    int32_t c;
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
} TirDependencies;

typedef struct {
    int32_t body_first;
    int32_t body_length;
    TirDependencies deps;
    int32_t local_count;
} LocalTir;

typedef struct {
    TirDependencies *global;
    LocalTir *thread;
} TirContext;

TirTag get_term_tag(TirContext ctx, TirId type);
TermData const *get_term_data(TirContext ctx, TirId term);
int32_t get_term_extra(TirContext ctx, int32_t index);

// Types

typedef struct {
    TirId index;
    TirId elem;
} ArrayType;

typedef struct {
    int32_t param_count;
    TirId *params;
    TirId ret;
} FunctionType;

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t field_count;
    TirId *fields;
} StructType;

typedef struct {
    int32_t scope;
    int32_t name;
    TirId repr;
} EnumType;

typedef struct {
    int32_t name;
    TirId inner;
    int32_t arg_count;
    TirId *args;
} TaggedType;

typedef struct {
    TirId inner;
    int32_t type_count;
    TirId *types;
} GenericTerm;

TirId new_array_type(TirContext ctx, ArrayType *t);
TirId new_array_length_type(TirContext ctx, int64_t length);
TirId new_ptr_type(TirContext ctx, TirTag tag, TirId elem);
TirId new_multiptr_type(TirContext ctx, TirTag tag, TirId elem);
TirId new_function_type(TirContext ctx, FunctionType *t);
TirId new_struct_type(TirContext ctx, Target target, StructType *t);
TirId new_enum_type(TirContext ctx, EnumType *t);
TirId new_linear_type(TirContext ctx, TirId elem);
TirId new_type_parameter(TirContext ctx, int32_t i, int32_t name);
TirId new_tagged_type(TirContext ctx, TaggedType *t);

TirId remove_any_pointer(TirContext ctx, TirId type);
TirId remove_pointer(TirContext ctx, TirId type);
TirId remove_slice(TirContext ctx, TirId type);
TirId replace_slice_with_pointer(TirContext ctx, TirId type);
TirId replace_pointer_with_slice(TirContext ctx, TirId type);
TirId remove_c_pointer_like(TirContext ctx, TirId type);
TirId remove_array_like(TirContext ctx, TirId type);
TirId remove_tags(TirContext ctx, TirId type);
bool is_aggregate_type(TirContext ctx, TirId type);
bool type_is_linear(TirContext ctx, TirId type);
bool type_is_unknown_size(TirContext ctx, TirId type);
bool is_equality_type(TirContext ctx, TirId a);
bool is_relative_type(TirContext ctx, TirId a);
bool int_fits_in_type(int64_t i, TirId type, Target target);
TirId bigger_primitive_type(TirId a, TirId b, Target target);

ArrayType get_array_type(TirContext ctx, TirId type);
int64_t get_array_length_type(TirContext ctx, TirId type);
TirId get_linear_elem_type(TirContext ctx, TirId type);
int32_t get_type_parameter_index(TirContext ctx, TirId type);
FunctionType get_function_type(TirContext ctx, TirId type);
TirId get_function_type_param(TirContext ctx, TirId type, int32_t index);
StructType get_struct_type(TirContext ctx, TirId type);
TirId get_struct_type_field(TirContext ctx, TirId type, int32_t index);
TirId get_any_struct_type_field(TirContext ctx, TirId type, int32_t index);
EnumType get_enum_type(TirContext ctx, TirId type);
TaggedType get_tagged_type(TirContext ctx, TirId type);
TirId get_tagged_type_arg(TirContext ctx, TirId type, int32_t index);
GenericTerm get_generic_term(TirContext ctx, TirId term);

int32_t sizeof_pointer(Target target);
int32_t alignof_type(TirContext ctx, TirId type, Target target);
int64_t sizeof_type(TirContext ctx, TirId type, Target target);
void print_type(FILE *file, TirContext ctx, TirId type);
void debug_type(TirContext ctx, TirId type);

static inline bool type_is_fixed_int(TirId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_char;
}

static inline bool type_is_int(TirId type) {
    return type.id >= TYPE_i8 && type.id <= TYPE_isize;
}

static inline bool type_is_float(TirId type) {
    return type.id >= TYPE_f32 && type.id <= TYPE_f64;
}

static inline bool type_is_arithmetic(TirId type) {
    return type_is_int(type) || type_is_float(type);
}

// Type matching

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

int match_types(TirContext ctx, TirId *results, int32_t count, TirId *types, TypeMatcher *matchers);
int match_type_parameters(TirContext ctx, TirId *results, TirId param, TirId arg);

typedef struct {
    TirContext ctx;
    TirId const *args;
    Arena scratch;
    Target target;
} ReplaceTypeInfo;

TirId replace_type_parameters(TirId generic, ReplaceTypeInfo *info);

#define match_ignore 0
#define match_array(I, E, EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_ARRAY, .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)}})
#define match_T(EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})

// Values

TirId new_int_constant(TirContext ctx, TirId type, int64_t x);
TirId new_float_constant(TirContext ctx, TirId type, double x);
TirId new_null_constant(TirContext ctx, TirId type);
TirId new_string_constant(TirContext ctx, TirId type, int32_t s);
TirId new_function(TirContext ctx, TirId type, int32_t name);
TirId new_extern_function(TirContext ctx, TirId type, int32_t name);
TirId new_extern_var(TirContext ctx, TirId type, int32_t name);
TirId new_variable(TirContext ctx, AstId node, TirId type, int32_t index, bool mutable);
TirId new_unary_tir(TirContext ctx, TirTag tag, AstId node, TirId type, TirId a);
TirId new_binary_tir(TirContext ctx, TirTag tag, AstId node, TirId type, TirId a, TirId b);
TirId new_instr(TirContext ctx, TirTag tag, AstId node, TirId type, int32_t a, int32_t b);
TirId new_generic(
    TirContext ctx,
    TirId inner,
    int32_t type_count,
    TirId *types
);

TirId get_value_type(TirContext ctx, TirId value);
ValueCategory get_value_category(TirContext ctx, TirId value);
char const *get_value_str(TirContext ctx, TirId value);
int64_t get_value_int(TirContext ctx, TirId value);
double get_value_float(TirContext ctx, TirId value);
