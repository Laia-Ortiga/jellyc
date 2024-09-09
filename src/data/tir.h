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
    TypeId *ptr;
} TypeSet;

typedef struct {
    int32_t index;
    int32_t extra;
} TypeData;

typedef struct {
    SumVec(TypeData) types;
    Vec(int32_t) extra;
    TypeSet set;
} TypeList;

typedef struct {
    TypeList const *global;
    TypeList thread;
} TypeSlice;

typedef struct {
    TypeId type;
    int32_t index;
} ValueData;

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
    SumVec(ValueData) values;
    Vec(uint64_t) extra;
} ValueList;

typedef struct {
    StringBuffer strtab;
    TypeList types;
    ValueList values;
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

// Types

TypeId new_array_type(TirContext ctx, TypeId index, TypeId element);
TypeId new_array_length_type(TirContext ctx, int64_t length);
TypeId new_ptr_type(TirContext ctx, TypeTag tag, TypeId elem);
TypeId new_multiptr_type(TirContext ctx, TypeTag tag, TypeId elem);
TypeId new_function_type(TirContext ctx, int32_t type_param_count, int32_t param_count, TypeId const *params, TypeId ret);
TypeId new_struct_type(TirContext ctx, int32_t scope, int32_t name, int32_t type_param_count, int32_t field_count, TypeId const *fields, Target target);
TypeId new_enum_type(TirContext ctx, int32_t scope, int32_t name, TypeId repr);
TypeId new_newtype_type(TirContext ctx, int32_t name, int32_t tags, TypeId type);
TypeId new_tagged_type(TirContext ctx, TypeId newtype, TypeId inner, int32_t arg_count, TypeId const *args);
TypeId new_linear_type(TirContext ctx, TypeId elem);
TypeId new_type_parameter(TirContext ctx, int32_t i, int32_t name);

TypeTag get_type_tag(TirContext ctx, TypeId type);
TypeId remove_any_pointer(TirContext ctx, TypeId type);
TypeId remove_pointer(TirContext ctx, TypeId type);
TypeId remove_slice(TirContext ctx, TypeId type);
TypeId replace_slice_with_pointer(TirContext ctx, TypeId type);
TypeId replace_pointer_with_slice(TirContext ctx, TypeId type);
TypeId remove_c_pointer_like(TirContext ctx, TypeId type);
TypeId remove_array_like(TirContext ctx, TypeId type);
TypeId remove_templ(TirContext ctx, TypeId type);
bool is_aggregate_type(TirContext ctx, TypeId type);
bool type_is_linear(TirContext ctx, TypeId type);
bool type_is_unknown_size(TirContext ctx, TypeId type);
bool type_eq(TirContext ctx, TypeId a, TypeId b);
bool is_equality_type(TirContext ctx, TypeId a);
bool is_relative_type(TirContext ctx, TypeId a);
bool int_fits_in_type(int64_t i, TypeId type, Target target);
TypeId bigger_primitive_type(TypeId a, TypeId b, Target target);

typedef struct {
    TypeId index;
    TypeId elem;
} ArrayType;

typedef struct {
    int32_t type_param_count;
    int32_t param_count;
    TypeId ret;
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
    TypeId repr;
} EnumType;

typedef struct {
    int32_t tags;
    int32_t name;
    TypeId type;
} NewtypeType;

typedef struct {
    TypeId newtype;
    TypeId inner;
    int32_t arg_count;
} TaggedType;

ArrayType get_array_type(TirContext ctx, TypeId type);
int64_t get_array_length_type(TirContext ctx, TypeId type);
TypeId get_linear_elem_type(TirContext ctx, TypeId type);
int32_t get_type_parameter_index(TirContext ctx, TypeId type);
FunctionType get_function_type(TirContext ctx, TypeId type);
TypeId get_function_type_param(TirContext ctx, TypeId type, int32_t index);
StructType get_struct_type(TirContext ctx, TypeId type);
TypeId get_struct_type_field(TirContext ctx, TypeId type, int32_t index);
TypeId get_any_struct_type_field(TirContext ctx, TypeId type, int32_t index);
EnumType get_enum_type(TirContext ctx, TypeId type);
NewtypeType get_newtype_type(TirContext ctx, TypeId type);
TaggedType get_tagged_type(TirContext ctx, TypeId type);
TypeId get_tagged_type_arg(TirContext ctx, TypeId type, int32_t index);

int32_t sizeof_pointer(Target target);
int32_t alignof_type(TirContext ctx, TypeId type, Target target);
int64_t sizeof_type(TirContext ctx, TypeId type, Target target);
void print_type(FILE *file, TirContext ctx, TypeId type);
void debug_type(TirContext ctx, TypeId type);

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

int match_types(TirContext ctx, TypeId *results, int32_t count, TypeId *types, TypeMatcher *matchers);
int match_type_parameters(TirContext ctx, TypeId *results, TypeId param, TypeId arg);
TypeId replace_type_parameters(TirContext ctx, TypeId *args, TypeId generic, Arena scratch);

#define match_ignore 0
#define match_array(I, E, EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_ARRAY, .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)}})
#define match_T(EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})

// Values

void init_tir_deps(TirDependencies *deps);
ValueId new_int_constant(TirContext ctx, TypeId type, int64_t x);
ValueId new_float_constant(TirContext ctx, TypeId type, double x);
ValueId new_null_constant(TirContext ctx, TypeId type);
ValueId new_string_constant(TirContext ctx, TypeId type, int32_t s);
ValueId new_function(TirContext ctx, TypeId type, int32_t name);
ValueId new_extern_function(TirContext ctx, TypeId type, int32_t name);
ValueId new_extern_var(TirContext ctx, TypeId type, int32_t name);
ValueId new_variable(TirContext ctx, TypeId type, bool mutable);
ValueId new_temporary(TirContext ctx, TypeId type, TirId tir_id);

ValueTag get_value_tag(TirContext ctx, ValueId value);
ValueData const *get_value_data(TirContext ctx, ValueId value);
TypeId get_value_type(TirContext ctx, ValueId value);
ValueCategory get_value_category(TirContext ctx, ValueId value);
char const *get_value_str(TirContext ctx, ValueId value);
int64_t get_value_int(TirContext ctx, ValueId value);
double get_value_float(TirContext ctx, ValueId value);

// Instructions

TirTag get_tir_tag(TirInstList *insts, TirId inst);
TirInstData get_tir_data(TirInstList *insts, TirId inst);
int32_t get_tir_extra(TirInstList *insts, int32_t index);
