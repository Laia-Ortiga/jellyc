#pragma once

#include "arena.h"
#include "ast.h"
#include "fwd.h"
#include "hash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    TIR_ERROR,

    TIR_TYPE_START,
    TIR_PRIMITIVE_TYPE = TIR_TYPE_START,
    TIR_ARRAY_TYPE,
    TIR_ARRAY_LENGTH_TYPE,
    TIR_PTR_TYPE,
    TIR_MUT_PTR_TYPE,
    TIR_SLICE_TYPE,
    TIR_MUT_SLICE_TYPE,
    TIR_FUNCTION_TYPE,
    TIR_TAGGED_TYPE,
    TIR_STRUCT_TYPE,
    TIR_ENUM_TYPE,
    TIR_LINEAR_TYPE,
    TIR_TYPE_PARAMETER,
    TIR_TYPE_END,

    TIR_VALUE_START = TIR_TYPE_END,
    TIR_FUNCTION = TIR_VALUE_START,

    TIR_EXTERN_FUNCTION,
    TIR_EXTERN_VAR,

    TIR_CONST_INT,
    TIR_CONST_FLOAT,
    TIR_CONST_NULL,
    TIR_STRING,

    TIR_VARIABLE,
    TIR_MUTABLE_VARIABLE,

    TIR_LET,
    TIR_MUT,

    TIR_PLUS,
    TIR_MINUS,
    TIR_NOT,
    TIR_DEREF,
    TIR_ADDRESS,
    TIR_ADDRESS_OF_TEMPORARY,

    TIR_ADD,
    TIR_SUB,
    TIR_MUL,
    TIR_DIV,
    TIR_MOD,

    TIR_AND,
    TIR_OR,
    TIR_XOR,
    TIR_SHL,
    TIR_SHR,

    TIR_EQ,
    TIR_NE,
    TIR_LT,
    TIR_GT,
    TIR_LE,
    TIR_GE,

    TIR_ASSIGN,
    TIR_ASSIGN_ADD,
    TIR_ASSIGN_SUB,
    TIR_ASSIGN_MUL,
    TIR_ASSIGN_DIV,
    TIR_ASSIGN_MOD,
    TIR_ASSIGN_AND,
    TIR_ASSIGN_OR,
    TIR_ASSIGN_XOR,

    TIR_ITOF,
    TIR_ITRUNC,
    TIR_SEXT,
    TIR_ZEXT,
    TIR_FTOI,
    TIR_FTRUNC,
    TIR_FEXT,
    TIR_PTR_CAST,
    TIR_NOP,
    TIR_ARRAY_TO_SLICE,

    TIR_CALL,
    TIR_INDEX,
    TIR_SLICE,
    TIR_ACCESS,
    TIR_NEW_STRUCT,
    TIR_NEW_ARRAY,

    TIR_IF,
    TIR_SWITCH,
    TIR_LOOP,
    TIR_BREAK,
    TIR_CONTINUE,
    TIR_RETURN,

    TIR_VALUE_END,

    TIR_MACRO = TIR_VALUE_END,
    TIR_MODULE,

    TIR_GENERIC,
} TirTag;

static inline bool is_tir_type(TirTag tag) {
    return tag >= TIR_TYPE_START && tag < TIR_TYPE_END;
}

static inline bool is_tir_value(TirTag tag) {
    return tag >= TIR_VALUE_START && tag < TIR_VALUE_END;
}

typedef enum {
    VALUE_INVALID,
    VALUE_TEMPORARY,
    VALUE_PLACE,
    VALUE_MUTABLE_PLACE,
    VALUE_MULTIVALUE,
} ValueCategory;

typedef struct {
    int32_t id;
} TirId;

static TirId const null_tir = {0};
#define ptype(type) ((TirId) {TYPE_##type})

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

TirTag get_term_tag(TirContext c, TirId type);
TermData const *get_term_data(TirContext c, TirId term);
int32_t get_term_extra(TirContext c, int32_t index);

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

TirId new_array_type(TirContext c, ArrayType *t);
TirId new_array_length_type(TirContext c, int64_t length);
TirId new_ptr_type(TirContext c, TirId elem);
TirId new_mut_ptr_type(TirContext c, TirId elem);
TirId new_slice_type(TirContext c, TirId elem);
TirId new_mut_slice_type(TirContext c, TirId elem);
TirId new_function_type(TirContext c, FunctionType *t);
TirId new_struct_type(TirContext c, Target target, StructType *t);
TirId new_enum_type(TirContext c, EnumType *t);
TirId new_linear_type(TirContext c, TirId elem);
TirId new_type_parameter(TirContext c, int32_t i, int32_t name);
TirId new_tagged_type(TirContext c, TaggedType *t);

ArrayType get_array_type(TirContext c, TirId type);
int64_t get_array_length_type(TirContext c, TirId type);
TirId get_linear_elem_type(TirContext c, TirId type);
int32_t get_type_parameter_index(TirContext c, TirId type);
FunctionType get_function_type(TirContext c, TirId type);
TirId get_function_type_param(TirContext c, TirId type, int32_t index);
StructType get_struct_type(TirContext c, TirId type);
TirId get_struct_type_field(TirContext c, TirId type, int32_t index);
TirId get_any_struct_type_field(TirContext c, TirId type, int32_t index);
EnumType get_enum_type(TirContext c, TirId type);
TaggedType get_tagged_type(TirContext c, TirId type);
TirId get_tagged_type_arg(TirContext c, TirId type, int32_t index);

TirId remove_any_pointer(TirContext c, TirId a);
TirId remove_pointer(TirContext c, TirId a);
TirId remove_slice(TirContext c, TirId a);
TirId replace_slice_with_pointer(TirContext c, TirId a);
TirId replace_pointer_with_slice(TirContext c, TirId a);
TirId remove_c_pointer_like(TirContext c, TirId a);
TirId remove_array_like(TirContext c, TirId a);
TirId remove_tags(TirContext c, TirId a);
bool is_aggregate_type(TirContext c, TirId a);
bool type_is_linear(TirContext c, TirId a);
bool type_is_unknown_size(TirContext c, TirId a);
bool is_equality_type(TirContext c, TirId a);
bool is_relative_type(TirContext c, TirId a);

bool int_fits_in_type(int64_t i, TirId a, Target target);
TirId bigger_primitive_type(TirId a, TirId b, Target target);
int32_t sizeof_pointer(Target target);
int32_t alignof_type(TirContext c, TirId type, Target target);
int64_t sizeof_type(TirContext c, TirId type, Target target);

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

void print_type(FILE *file, TirContext c, TirId type);
void debug_type(TirContext c, TirId type);

// Type Matching

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
#define match_array(I, E, EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_ARRAY, .extra = (EXTRA), .inner = (TypeMatcher[]) {(I), (E)}})
#define match_T(EXTRA) ((TypeMatcher) {.match_type = TYPE_MATCH_T, .extra = (EXTRA)})

// Values

TirId new_int_constant(TirContext c, TirId type, int64_t x);
TirId new_float_constant(TirContext c, TirId type, double x);
TirId new_null_constant(TirContext c, TirId type);
TirId new_string_constant(TirContext c, TirId type, int32_t s);
TirId new_function(TirContext c, TirId type, int32_t name);
TirId new_extern_function(TirContext c, TirId type, int32_t name);
TirId new_extern_var(TirContext c, TirId type, int32_t name);
TirId new_variable(TirContext c, AstId node, TirId type, int32_t index, bool mutable);
TirId new_unary_tir(TirContext c, TirTag tag, AstId node, TirId type, TirId a);
TirId new_binary_tir(TirContext c, TirTag tag, AstId node, TirId type, TirId a, TirId b);
TirId new_instr(TirContext c, TirTag tag, AstId node, TirId type, int32_t a, int32_t b);

TirId get_value_type(TirContext c, TirId value);
ValueCategory get_value_category(TirContext c, TirId value);
char const *get_value_str(TirContext c, TirId value);
int64_t get_value_int(TirContext c, TirId value);
double get_value_float(TirContext c, TirId value);

// Other

TirId new_generic(
    TirContext c,
    TirId inner,
    int32_t type_count,
    TirId *types
);
GenericTerm get_generic_term(TirContext c, TirId term);
