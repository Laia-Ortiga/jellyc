#pragma once

#include "arena.h"
#include "fwd.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    size_t capacity;
    size_t count;
    TermId *ptr;
} TermSet;

typedef struct {
    int32_t a;
    int32_t b;
} TermData;

typedef struct {
    AstId node;
    int32_t left;
    int32_t right;
} TirInstData;

typedef struct {
    SumVec(TirInstData) insts;
    Vec(int32_t) extra;
} TirInstList;

typedef struct {
    SumVec(TermData) terms;
    Vec(int32_t) extra;
    TermSet set;
} TermList;

typedef struct {
    StringBuffer strtab;
    TermList terms;
} TirDependencies;

typedef struct {
    TirInstList insts;
    TirId first;
    TirDependencies deps;
    int32_t local_count;
} LocalTir;

typedef struct {
    TirDependencies *global;
    LocalTir *thread;
} TirContext;

typedef struct {
    TirContext ctx;
    TirInstList insts;
} Tir;

TermTag get_term_tag(TirContext ctx, TermId type);
TermData const *get_term_data(TirContext ctx, TermId term);

// Types

TermId new_array_type(TirContext ctx, TermId index, TermId element);
TermId new_array_length_type(TirContext ctx, int64_t length);
TermId new_ptr_type(TirContext ctx, TermTag tag, TermId elem);
TermId new_multiptr_type(TirContext ctx, TermTag tag, TermId elem);
TermId new_function_type(TirContext ctx, int32_t type_param_count, int32_t param_count, TermId const *params, TermId ret);
TermId new_struct_type(TirContext ctx, int32_t scope, int32_t name, int32_t type_param_count, int32_t field_count, TermId const *fields, Target target);
TermId new_enum_type(TirContext ctx, int32_t scope, int32_t name, TermId repr);
TermId new_newtype_type(TirContext ctx, int32_t name, int32_t tags, TermId type);
TermId new_tagged_type(TirContext ctx, TermId newtype, TermId inner, int32_t arg_count, TermId const *args);
TermId new_linear_type(TirContext ctx, TermId elem);
TermId new_type_parameter(TirContext ctx, int32_t i, int32_t name);

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

typedef struct {
    TermId index;
    TermId elem;
} ArrayType;

typedef struct {
    int32_t type_param_count;
    int32_t param_count;
    TermId *params;
    TermId ret;
} FunctionType;

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t alignment;
    int64_t size;
    int32_t type_param_count;
    int32_t field_count;
    bool is_linear;
} StructType;

typedef struct {
    int32_t scope;
    int32_t name;
    TermId repr;
} EnumType;

typedef struct {
    int32_t tags;
    int32_t name;
    TermId type;
} NewtypeType;

typedef struct {
    TermId newtype;
    TermId inner;
    int32_t arg_count;
    TermId *args;
} TaggedType;

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
NewtypeType get_newtype_type(TirContext ctx, TermId type);
TaggedType get_tagged_type(TirContext ctx, TermId type);
TermId get_tagged_type_arg(TirContext ctx, TermId type, int32_t index);

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
TermId replace_type_parameters(TirContext ctx, TermId *args, TermId generic, Arena scratch);

#define match_ignore 0
#define match_array(I, E, EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_ARRAY, .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)}})
#define match_T(EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})

// Values

void init_tir_deps(TirDependencies *deps);
TermId new_int_constant(TirContext ctx, TermId type, int64_t x);
TermId new_float_constant(TirContext ctx, TermId type, double x);
TermId new_null_constant(TirContext ctx, TermId type);
TermId new_string_constant(TirContext ctx, TermId type, int32_t s);
TermId new_function(TirContext ctx, TermId type, int32_t name);
TermId new_extern_function(TirContext ctx, TermId type, int32_t name);
TermId new_extern_var(TirContext ctx, TermId type, int32_t name);
TermId new_variable(TirContext ctx, TermId type, bool mutable);
TermId new_temporary(TirContext ctx, TermId type, TirId tir_id);

TermId get_value_type(TirContext ctx, TermId value);
ValueCategory get_value_category(TirContext ctx, TermId value);
char const *get_value_str(TirContext ctx, TermId value);
int64_t get_value_int(TirContext ctx, TermId value);
double get_value_float(TirContext ctx, TermId value);

// Instructions

TirTag get_tir_tag(TirInstList *insts, TirId inst);
TirInstData get_tir_data(TirInstList *insts, TirId inst);
int32_t get_tir_extra(TirInstList *insts, int32_t index);
