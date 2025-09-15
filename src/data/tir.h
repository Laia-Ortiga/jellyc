#pragma once

#include "arena.h"
#include "enums.h"
#include "fwd.h"
#include "wrappers.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    int32_t capacity;
    int32_t count;
    TermId *ptr;
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
        TermId enum_value;
    };
} TypeScopeSymbol;

typedef struct {
    StringBuffer strtab;
    TermList terms;
    TermId main;
    Vec(TermId) structs;
    Vec(TermId) extern_vars;
    Vec(TermId) extern_functions;
    Vec(TermId) functions;
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

TermTag get_term_tag(TirContext ctx, TermId type);
TermData const *get_term_data(TirContext ctx, TermId term);
int32_t get_term_extra(TirContext ctx, int32_t index);

// Types

typedef struct {
    TermId index;
    TermId elem;
} ArrayType;

typedef struct {
    int32_t param_count;
    TermId *params;
    TermId ret;
} FunctionType;

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t field_count;
    TermId *fields;
} StructType;

typedef struct {
    int32_t scope;
    int32_t name;
    TermId repr;
} EnumType;

typedef struct {
    int32_t name;
    TermId inner;
    int32_t arg_count;
    TermId *args;
} TaggedType;

typedef struct {
    TermId inner;
    int32_t type_count;
    TermId *types;
} GenericTerm;

TermId new_array_type(TirContext ctx, ArrayType *t);
TermId new_array_length_type(TirContext ctx, int64_t length);
TermId new_ptr_type(TirContext ctx, TermTag tag, TermId elem);
TermId new_multiptr_type(TirContext ctx, TermTag tag, TermId elem);
TermId new_function_type(TirContext ctx, FunctionType *t);
TermId new_struct_type(TirContext ctx, Target target, StructType *t);
TermId new_enum_type(TirContext ctx, EnumType *t);
TermId new_linear_type(TirContext ctx, TermId elem);
TermId new_type_parameter(TirContext ctx, int32_t i, int32_t name);
TermId new_tagged_type(TirContext ctx, TaggedType *t);

TermId remove_any_pointer(TirContext ctx, TermId type);
TermId remove_pointer(TirContext ctx, TermId type);
TermId remove_slice(TirContext ctx, TermId type);
TermId replace_slice_with_pointer(TirContext ctx, TermId type);
TermId replace_pointer_with_slice(TirContext ctx, TermId type);
TermId remove_c_pointer_like(TirContext ctx, TermId type);
TermId remove_array_like(TirContext ctx, TermId type);
TermId remove_tags(TirContext ctx, TermId type);
bool is_aggregate_type(TirContext ctx, TermId type);
bool type_is_linear(TirContext ctx, TermId type);
bool type_is_unknown_size(TirContext ctx, TermId type);
bool is_equality_type(TirContext ctx, TermId a);
bool is_relative_type(TirContext ctx, TermId a);
bool int_fits_in_type(int64_t i, TermId type, Target target);
TermId bigger_primitive_type(TermId a, TermId b, Target target);

ArrayType get_array_type(TirContext ctx, TermId type);
int64_t get_array_length_type(TirContext ctx, TermId type);
TermId get_linear_elem_type(TirContext ctx, TermId type);
int32_t get_type_parameter_index(TirContext ctx, TermId type);
FunctionType get_function_type(TirContext ctx, TermId type);
TermId get_function_type_param(TirContext ctx, TermId type, int32_t index);
StructType get_struct_type(TirContext ctx, TermId type);
TermId get_struct_type_field(TirContext ctx, TermId type, int32_t index);
TermId get_any_struct_type_field(TirContext ctx, TermId type, int32_t index);
EnumType get_enum_type(TirContext ctx, TermId type);
TaggedType get_tagged_type(TirContext ctx, TermId type);
TermId get_tagged_type_arg(TirContext ctx, TermId type, int32_t index);
GenericTerm get_generic_term(TirContext ctx, TermId term);

int32_t sizeof_pointer(Target target);
int32_t alignof_type(TirContext ctx, TermId type, Target target);
int64_t sizeof_type(TirContext ctx, TermId type, Target target);
void print_type(FILE *file, TirContext ctx, TermId type);
void debug_type(TirContext ctx, TermId type);

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

int match_types(TirContext ctx, TermId *results, int32_t count, TermId *types, TypeMatcher *matchers);
int match_type_parameters(TirContext ctx, TermId *results, TermId param, TermId arg);

typedef struct {
    TirContext ctx;
    TermId const *args;
    Arena scratch;
    Target target;
} ReplaceTypeInfo;

TermId replace_type_parameters(TermId generic, ReplaceTypeInfo *info);

#define match_ignore 0
#define match_array(I, E, EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_ARRAY, .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)}})
#define match_T(EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})

// Values

TermId new_int_constant(TirContext ctx, TermId type, int64_t x);
TermId new_float_constant(TirContext ctx, TermId type, double x);
TermId new_null_constant(TirContext ctx, TermId type);
TermId new_string_constant(TirContext ctx, TermId type, int32_t s);
TermId new_function(TirContext ctx, TermId type, int32_t name);
TermId new_extern_function(TirContext ctx, TermId type, int32_t name);
TermId new_extern_var(TirContext ctx, TermId type, int32_t name);
TermId new_variable(TirContext ctx, AstId node, TermId type, int32_t index, bool mutable);
TermId new_unary_tir(TirContext ctx, TermTag tag, AstId node, TermId type, TermId a);
TermId new_binary_tir(TirContext ctx, TermTag tag, AstId node, TermId type, TermId a, TermId b);
TermId new_instr(TirContext ctx, TermTag tag, AstId node, TermId type, int32_t a, int32_t b);
TermId new_generic(
    TirContext ctx,
    TermId inner,
    int32_t type_count,
    TermId *types
);

TermId get_value_type(TirContext ctx, TermId value);
ValueCategory get_value_category(TirContext ctx, TermId value);
char const *get_value_str(TirContext ctx, TermId value);
int64_t get_value_int(TirContext ctx, TermId value);
double get_value_float(TirContext ctx, TermId value);
