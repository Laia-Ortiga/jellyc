typedef enum {
    TIR_ERROR,
    TIR_RESERVED,
    TIR_GENERIC,
    TIR_BLOCK,
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
    TIR_AFFINE_TYPE,
    TIR_TYPE_PARAMETER,
    TIR_FUNCTION,
    TIR_EXTERN_FUNCTION,
    TIR_EXTERN_VAR,
    TIR_INT,
    TIR_FLOAT,
    TIR_NULL,
    TIR_STRING,
    TIR_PARAMETER,
    TIR_VARIABLE,
    TIR_MUTABLE_VARIABLE,
    TIR_LET,
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
    TIR_INARROW,
    TIR_SEXT,
    TIR_ZEXT,
    TIR_FTOI,
    TIR_FTRUNC,
    TIR_FEXT,
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
} TirTag;

typedef struct {
    AstId node;
    TirId inner;
    struct { int32_t len; TirId *ptr; } params;
} TirGeneric;

typedef struct {
    AstId node;
    struct { int32_t len; TirId *ptr; } stmts;
} TirBlock;

typedef struct {
    TirId elem;
    TirId index;
} TirArrayType;

typedef struct {
    int64_t length;
} TirArrayLengthType;

typedef struct {
    TirId elem;
} TirPtrType;

typedef struct {
    TirId elem;
    TirId cached_ptr;
} TirSliceType;

typedef struct {
    TirId ret;
    struct { int32_t len; TirId *ptr; } params;
} TirFunctionType;

typedef struct {
    int32_t name;
    TirId inner;
    struct { int32_t len; TirId *ptr; } args;
} TirTaggedType;

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t has_public_fields;
    FileId file;
    struct { int32_t len; TirId *ptr; } fields;
    int32_t alignment;
    int64_t size;
    int32_t is_affine;
} TirStructType;

typedef struct {
    int32_t scope;
    int32_t name;
    TirId repr;
} TirEnumType;

typedef struct {
    TirId elem;
} TirAffineType;

typedef struct {
    int32_t index;
    int32_t name;
} TirTypeParameter;

typedef struct {
    AstId node;
    TirId type;
    int32_t name;
} TirFunction;

typedef struct {
    AstId node;
    TirId type;
    int32_t name;
} TirExternFunction;

typedef struct {
    AstId node;
    TirId type;
    int32_t name;
} TirExternVar;

typedef struct {
    AstId node;
    TirId type;
    int64_t value;
} TirInt;

typedef struct {
    AstId node;
    TirId type;
    double value;
} TirFloat;

typedef struct {
    AstId node;
    TirId type;
} TirNull;

typedef struct {
    AstId node;
    TirId type;
    int32_t value;
} TirString;

typedef struct {
    AstId node;
    TirId type;
    int32_t index;
} TirVariable;

typedef struct {
    AstId node;
    TirId type;
    TirId var;
    TirId init;
} TirLet;

typedef struct {
    AstId node;
    TirId type;
    TirId a;
} TirUnary;

typedef struct {
    AstId node;
    TirId type;
    TirId a;
    TirId b;
} TirBinary;

typedef struct {
    AstId node;
    TirId type;
    TirId a;
} TirCast;

typedef struct {
    AstId node;
    TirId type;
    TirId f;
    struct { int32_t len; TirId *ptr; } args;
} TirCall;

typedef struct {
    AstId node;
    TirId type;
    TirId a;
    TirId index;
} TirIndex;

typedef struct {
    AstId node;
    TirId type;
    TirId a;
    TirId low;
    TirId high;
} TirSlice;

typedef struct {
    AstId node;
    TirId type;
    TirId s;
    int32_t field;
} TirAccess;

typedef struct {
    AstId node;
    TirId type;
    struct { int32_t len; TirId *ptr; } fields;
} TirNewStruct;

typedef struct {
    AstId node;
    TirId type;
    struct { int32_t len; TirId *ptr; } args;
} TirNewArray;

typedef struct {
    AstId node;
    TirId type;
    TirId condition;
    struct { int32_t len; TirId *ptr; } true_block;
    struct { int32_t len; TirId *ptr; } false_block;
} TirIf;

typedef struct {
    AstId node;
    TirId type;
    TirId condition;
    struct { int32_t len; TirId *ptr; } branches;
} TirSwitch;

typedef struct {
    AstId node;
    TirId type;
    TirId init;
    TirId condition;
    TirId next;
    struct { int32_t len; TirId *ptr; } block;
} TirLoop;

typedef struct {
    AstId node;
    TirId type;
} TirBreak;

typedef struct {
    AstId node;
    TirId type;
} TirContinue;

typedef struct {
    AstId node;
    TirId type;
    TirId value;
} TirReturn;

TirId tir_push_generic(TirContext c, TirGeneric a);
TirId tir_push_block(TirContext c, TirBlock a);
TirId tir_push_array_type(TirContext c, TirArrayType a);
TirId tir_push_array_length_type(TirContext c, TirArrayLengthType a);
TirId tir_push_ptr_type(TirContext c, TirTag tag, TirPtrType a);
TirId tir_push_slice_type(TirContext c, TirTag tag, TirSliceType a);
TirId tir_push_function_type(TirContext c, TirFunctionType a);
TirId tir_push_tagged_type(TirContext c, TirTaggedType a);
TirId tir_push_struct_type(TirContext c, TirStructType a);
TirId tir_push_enum_type(TirContext c, TirEnumType a);
TirId tir_push_affine_type(TirContext c, TirAffineType a);
TirId tir_push_type_parameter(TirContext c, TirTypeParameter a);
TirId tir_push_function(TirContext c, TirFunction a);
TirId tir_push_extern_function(TirContext c, TirExternFunction a);
TirId tir_push_extern_var(TirContext c, TirExternVar a);
TirId tir_push_int(TirContext c, TirInt a);
TirId tir_push_float(TirContext c, TirFloat a);
TirId tir_push_null(TirContext c, TirNull a);
TirId tir_push_string(TirContext c, TirString a);
TirId tir_push_variable(TirContext c, TirTag tag, TirVariable a);
TirId tir_push_let(TirContext c, TirLet a);
TirId tir_push_unary(TirContext c, TirTag tag, TirUnary a);
TirId tir_push_binary(TirContext c, TirTag tag, TirBinary a);
TirId tir_push_cast(TirContext c, TirTag tag, TirCast a);
TirId tir_push_call(TirContext c, TirCall a);
TirId tir_push_index(TirContext c, TirIndex a);
TirId tir_push_slice(TirContext c, TirSlice a);
TirId tir_push_access(TirContext c, TirAccess a);
TirId tir_push_new_struct(TirContext c, TirNewStruct a);
TirId tir_push_new_array(TirContext c, TirNewArray a);
TirId tir_push_if(TirContext c, TirIf a);
TirId tir_push_switch(TirContext c, TirSwitch a);
TirId tir_push_loop(TirContext c, TirLoop a);
TirId tir_push_break(TirContext c, TirBreak a);
TirId tir_push_continue(TirContext c, TirContinue a);
TirId tir_push_return(TirContext c, TirReturn a);

TirGeneric tir_get_generic(TirContext c, TirId a);
TirBlock tir_get_block(TirContext c, TirId a);
TirArrayType tir_get_array_type(TirContext c, TirId a);
TirArrayLengthType tir_get_array_length_type(TirContext c, TirId a);
TirPtrType tir_get_ptr_type(TirContext c, TirId a);
TirSliceType tir_get_slice_type(TirContext c, TirId a);
TirFunctionType tir_get_function_type(TirContext c, TirId a);
TirTaggedType tir_get_tagged_type(TirContext c, TirId a);
TirStructType tir_get_struct_type(TirContext c, TirId a);
TirEnumType tir_get_enum_type(TirContext c, TirId a);
TirAffineType tir_get_affine_type(TirContext c, TirId a);
TirTypeParameter tir_get_type_parameter(TirContext c, TirId a);
TirFunction tir_get_function(TirContext c, TirId a);
TirExternFunction tir_get_extern_function(TirContext c, TirId a);
TirExternVar tir_get_extern_var(TirContext c, TirId a);
TirInt tir_get_int(TirContext c, TirId a);
TirFloat tir_get_float(TirContext c, TirId a);
TirNull tir_get_null(TirContext c, TirId a);
TirString tir_get_string(TirContext c, TirId a);
TirVariable tir_get_variable(TirContext c, TirId a);
TirLet tir_get_let(TirContext c, TirId a);
TirUnary tir_get_unary(TirContext c, TirId a);
TirBinary tir_get_binary(TirContext c, TirId a);
TirCast tir_get_cast(TirContext c, TirId a);
TirCall tir_get_call(TirContext c, TirId a);
TirIndex tir_get_index(TirContext c, TirId a);
TirSlice tir_get_slice(TirContext c, TirId a);
TirAccess tir_get_access(TirContext c, TirId a);
TirNewStruct tir_get_new_struct(TirContext c, TirId a);
TirNewArray tir_get_new_array(TirContext c, TirId a);
TirIf tir_get_if(TirContext c, TirId a);
TirSwitch tir_get_switch(TirContext c, TirId a);
TirLoop tir_get_loop(TirContext c, TirId a);
TirBreak tir_get_break(TirContext c, TirId a);
TirContinue tir_get_continue(TirContext c, TirId a);
TirReturn tir_get_return(TirContext c, TirId a);

#define tir_push(c, ...) \
    (_Generic((__VA_ARGS__), \
        TirGeneric: tir_push_generic, \
        TirBlock: tir_push_block, \
        TirArrayType: tir_push_array_type, \
        TirArrayLengthType: tir_push_array_length_type, \
        TirFunctionType: tir_push_function_type, \
        TirTaggedType: tir_push_tagged_type, \
        TirStructType: tir_push_struct_type, \
        TirEnumType: tir_push_enum_type, \
        TirAffineType: tir_push_affine_type, \
        TirTypeParameter: tir_push_type_parameter, \
        TirFunction: tir_push_function, \
        TirExternFunction: tir_push_extern_function, \
        TirExternVar: tir_push_extern_var, \
        TirInt: tir_push_int, \
        TirFloat: tir_push_float, \
        TirNull: tir_push_null, \
        TirString: tir_push_string, \
        TirLet: tir_push_let, \
        TirCall: tir_push_call, \
        TirIndex: tir_push_index, \
        TirSlice: tir_push_slice, \
        TirAccess: tir_push_access, \
        TirNewStruct: tir_push_new_struct, \
        TirNewArray: tir_push_new_array, \
        TirIf: tir_push_if, \
        TirSwitch: tir_push_switch, \
        TirLoop: tir_push_loop, \
        TirBreak: tir_push_break, \
        TirContinue: tir_push_continue, \
        TirReturn: tir_push_return \
    )(c, __VA_ARGS__))

#define tir_push_tag(c, tag, ...) \
    (_Generic((__VA_ARGS__), \
        TirPtrType: tir_push_ptr_type, \
        TirSliceType: tir_push_slice_type, \
        TirVariable: tir_push_variable, \
        TirUnary: tir_push_unary, \
        TirBinary: tir_push_binary, \
        TirCast: tir_push_cast \
    )(c, tag, __VA_ARGS__))

#define TIR_TYPE_START 4
#define TIR_TYPE_END 16
#define TIR_VALUE_START 16
#define TIR_VALUE_END 80