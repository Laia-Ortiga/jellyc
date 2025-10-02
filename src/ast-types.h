typedef enum {
    AST_ROOT,
    AST_IMPORT,
    AST_PUBLIC,
    AST_FUNCTION,
    AST_ENUM,
    AST_STRUCT,
    AST_NEWTYPE,
    AST_CONST,
    AST_EXTERN_FUNCTION,
    AST_EXTERN_VAR,
    AST_PARAM,
    AST_LET,
    AST_MUT,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_SWITCH,
    AST_SWITCH_CASE,
    AST_BREAK,
    AST_CONTINUE,
    AST_RETURN,
    AST_ARRAY_TYPE,
    AST_ARRAY_TYPE_SUGAR,
    AST_PTR_TYPE,
    AST_MUT_PTR_TYPE,
    AST_SLICE_TYPE,
    AST_MUT_SLICE_TYPE,
    AST_PLUS,
    AST_MINUS,
    AST_NOT,
    AST_ADDRESS,
    AST_DEREF,
    AST_ADD,
    AST_SUB,
    AST_MUL,
    AST_DIV,
    AST_MOD,
    AST_AND,
    AST_OR,
    AST_XOR,
    AST_SHL,
    AST_SHR,
    AST_LOGIC_AND,
    AST_LOGIC_OR,
    AST_EQ,
    AST_NE,
    AST_LT,
    AST_GT,
    AST_LE,
    AST_GE,
    AST_ASSIGN,
    AST_ASSIGN_ADD,
    AST_ASSIGN_SUB,
    AST_ASSIGN_MUL,
    AST_ASSIGN_DIV,
    AST_ASSIGN_MOD,
    AST_ASSIGN_AND,
    AST_ASSIGN_OR,
    AST_ASSIGN_XOR,
    AST_FUNCTION_TYPE,
    AST_TYPE_HINT,
    AST_CALL,
    AST_INDEX,
    AST_SLICE,
    AST_ACCESS,
    AST_INFERRED_ACCESS,
    AST_LIST,
    AST_MAP_ENTRY,
    AST_MAP,
    AST_BLOCK,
    AST_ID,
    AST_INT,
    AST_FLOAT,
    AST_CHAR,
    AST_STRING,
    AST_TRUE,
    AST_FALSE,
    AST_NULL,
} AstTag;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } defs;
} AstRoot;

typedef struct {
    SourceIndex token;
} AstImport;

typedef struct {
    SourceIndex token;
    AstId def;
} AstPublic;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } type_params;
    struct { int32_t len; AstId *ptr; } params;
    AstId ret;
    AstId body;
} AstFunction;

typedef struct {
    SourceIndex token;
    AstId repr;
    struct { int32_t len; AstId *ptr; } members;
} AstEnum;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } type_params;
    struct { int32_t len; AstId *ptr; } fields;
} AstStruct;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } type_params;
    AstId type;
} AstNewtype;

typedef struct {
    SourceIndex token;
    AstId init;
} AstConst;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } params;
    AstId ret;
} AstExternFunction;

typedef struct {
    SourceIndex token;
    AstId type;
} AstExternVar;

typedef struct {
    SourceIndex token;
    AstId type;
} AstParam;

typedef struct {
    SourceIndex token;
    AstId init;
} AstLet;

typedef struct {
    SourceIndex token;
    AstId condition;
    AstId true_block;
    AstId false_block;
} AstIf;

typedef struct {
    SourceIndex token;
    AstId condition;
    AstId block;
} AstWhile;

typedef struct {
    SourceIndex token;
    AstId init;
    AstId condition;
    AstId next;
    AstId block;
} AstFor;

typedef struct {
    SourceIndex token;
    AstId condition;
    struct { int32_t len; AstId *ptr; } branches;
} AstSwitch;

typedef struct {
    SourceIndex token;
    AstId pattern;
    AstId value;
} AstSwitchCase;

typedef struct {
    SourceIndex token;
} AstBreak;

typedef struct {
    SourceIndex token;
} AstContinue;

typedef struct {
    SourceIndex token;
    AstId value;
} AstReturn;

typedef struct {
    SourceIndex token;
    AstId index;
    AstId elem;
} AstArrayType;

typedef struct {
    SourceIndex token;
    AstId length;
    AstId elem;
} AstArrayTypeSugar;

typedef struct {
    SourceIndex token;
    AstId a;
} AstUnary;

typedef struct {
    SourceIndex token;
    AstId a;
    AstId b;
} AstBinary;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } params;
    AstId ret;
} AstFunctionType;

typedef struct {
    SourceIndex token;
    AstId type;
    AstId value;
} AstTypeHint;

typedef struct {
    SourceIndex token;
    AstId a;
    struct { int32_t len; AstId *ptr; } args;
} AstCall;

typedef struct {
    SourceIndex token;
    AstId s;
} AstAccess;

typedef struct {
    SourceIndex token;
} AstInferredAccess;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } elems;
} AstList;

typedef struct {
    SourceIndex token;
    AstId value;
} AstMapEntry;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } entries;
} AstMap;

typedef struct {
    SourceIndex token;
    struct { int32_t len; AstId *ptr; } stmts;
} AstBlock;

typedef struct {
    SourceIndex token;
} AstLeaf;

AstId ast_push_root(Ast *c, AstRoot a);
AstId ast_push_import(Ast *c, AstImport a);
AstId ast_push_public(Ast *c, AstPublic a);
AstId ast_push_function(Ast *c, AstFunction a);
AstId ast_push_enum(Ast *c, AstEnum a);
AstId ast_push_struct(Ast *c, AstStruct a);
AstId ast_push_newtype(Ast *c, AstNewtype a);
AstId ast_push_const(Ast *c, AstConst a);
AstId ast_push_extern_function(Ast *c, AstExternFunction a);
AstId ast_push_extern_var(Ast *c, AstExternVar a);
AstId ast_push_param(Ast *c, AstParam a);
AstId ast_push_let(Ast *c, AstTag tag, AstLet a);
AstId ast_push_if(Ast *c, AstIf a);
AstId ast_push_while(Ast *c, AstWhile a);
AstId ast_push_for(Ast *c, AstFor a);
AstId ast_push_switch(Ast *c, AstSwitch a);
AstId ast_push_switch_case(Ast *c, AstSwitchCase a);
AstId ast_push_break(Ast *c, AstBreak a);
AstId ast_push_continue(Ast *c, AstContinue a);
AstId ast_push_return(Ast *c, AstReturn a);
AstId ast_push_array_type(Ast *c, AstArrayType a);
AstId ast_push_array_type_sugar(Ast *c, AstArrayTypeSugar a);
AstId ast_push_unary(Ast *c, AstTag tag, AstUnary a);
AstId ast_push_binary(Ast *c, AstTag tag, AstBinary a);
AstId ast_push_function_type(Ast *c, AstFunctionType a);
AstId ast_push_type_hint(Ast *c, AstTypeHint a);
AstId ast_push_call(Ast *c, AstTag tag, AstCall a);
AstId ast_push_access(Ast *c, AstAccess a);
AstId ast_push_inferred_access(Ast *c, AstInferredAccess a);
AstId ast_push_list(Ast *c, AstList a);
AstId ast_push_map_entry(Ast *c, AstMapEntry a);
AstId ast_push_map(Ast *c, AstMap a);
AstId ast_push_block(Ast *c, AstBlock a);
AstId ast_push_leaf(Ast *c, AstTag tag, AstLeaf a);

AstRoot ast_get_root(Ast *c, AstId a);
AstImport ast_get_import(Ast *c, AstId a);
AstPublic ast_get_public(Ast *c, AstId a);
AstFunction ast_get_function(Ast *c, AstId a);
AstEnum ast_get_enum(Ast *c, AstId a);
AstStruct ast_get_struct(Ast *c, AstId a);
AstNewtype ast_get_newtype(Ast *c, AstId a);
AstConst ast_get_const(Ast *c, AstId a);
AstExternFunction ast_get_extern_function(Ast *c, AstId a);
AstExternVar ast_get_extern_var(Ast *c, AstId a);
AstParam ast_get_param(Ast *c, AstId a);
AstLet ast_get_let(Ast *c, AstId a);
AstIf ast_get_if(Ast *c, AstId a);
AstWhile ast_get_while(Ast *c, AstId a);
AstFor ast_get_for(Ast *c, AstId a);
AstSwitch ast_get_switch(Ast *c, AstId a);
AstSwitchCase ast_get_switch_case(Ast *c, AstId a);
AstBreak ast_get_break(Ast *c, AstId a);
AstContinue ast_get_continue(Ast *c, AstId a);
AstReturn ast_get_return(Ast *c, AstId a);
AstArrayType ast_get_array_type(Ast *c, AstId a);
AstArrayTypeSugar ast_get_array_type_sugar(Ast *c, AstId a);
AstUnary ast_get_unary(Ast *c, AstId a);
AstBinary ast_get_binary(Ast *c, AstId a);
AstFunctionType ast_get_function_type(Ast *c, AstId a);
AstTypeHint ast_get_type_hint(Ast *c, AstId a);
AstCall ast_get_call(Ast *c, AstId a);
AstAccess ast_get_access(Ast *c, AstId a);
AstInferredAccess ast_get_inferred_access(Ast *c, AstId a);
AstList ast_get_list(Ast *c, AstId a);
AstMapEntry ast_get_map_entry(Ast *c, AstId a);
AstMap ast_get_map(Ast *c, AstId a);
AstBlock ast_get_block(Ast *c, AstId a);
AstLeaf ast_get_leaf(Ast *c, AstId a);

#define ast_push(c, ...) \
    (_Generic((__VA_ARGS__), \
        AstRoot: ast_push_root, \
        AstImport: ast_push_import, \
        AstPublic: ast_push_public, \
        AstFunction: ast_push_function, \
        AstEnum: ast_push_enum, \
        AstStruct: ast_push_struct, \
        AstNewtype: ast_push_newtype, \
        AstConst: ast_push_const, \
        AstExternFunction: ast_push_extern_function, \
        AstExternVar: ast_push_extern_var, \
        AstParam: ast_push_param, \
        AstIf: ast_push_if, \
        AstWhile: ast_push_while, \
        AstFor: ast_push_for, \
        AstSwitch: ast_push_switch, \
        AstSwitchCase: ast_push_switch_case, \
        AstBreak: ast_push_break, \
        AstContinue: ast_push_continue, \
        AstReturn: ast_push_return, \
        AstArrayType: ast_push_array_type, \
        AstArrayTypeSugar: ast_push_array_type_sugar, \
        AstFunctionType: ast_push_function_type, \
        AstTypeHint: ast_push_type_hint, \
        AstAccess: ast_push_access, \
        AstInferredAccess: ast_push_inferred_access, \
        AstList: ast_push_list, \
        AstMapEntry: ast_push_map_entry, \
        AstMap: ast_push_map, \
        AstBlock: ast_push_block \
    )(c, __VA_ARGS__))

#define ast_push_tag(c, tag, ...) \
    (_Generic((__VA_ARGS__), \
        AstLet: ast_push_let, \
        AstUnary: ast_push_unary, \
        AstBinary: ast_push_binary, \
        AstCall: ast_push_call, \
        AstLeaf: ast_push_leaf \
    )(c, tag, __VA_ARGS__))
