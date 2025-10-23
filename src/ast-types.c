#include "ast.h"

#include <stdlib.h>

AstId ast_push_root(Ast *c, AstRoot a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.defs.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.defs.len * (sizeof(a.defs.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, a.defs.ptr, a.defs.len * sizeof(a.defs.ptr[0]));
    extra += a.defs.len * (sizeof(a.defs.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_ROOT, data);
}

AstId ast_push_import(Ast *c, AstImport a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    return ast_new(c, AST_IMPORT, data);
}

AstId ast_push_public(Ast *c, AstPublic a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.def + 0, sizeof(int32_t));
    return ast_new(c, AST_PUBLIC, data);
}

AstId ast_push_function(Ast *c, AstFunction a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type_params.len + 0, sizeof(int32_t));
    int32_t n = 3;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.params.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.ret + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.body + 0, sizeof(int32_t));
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_FUNCTION, data);
}

AstId ast_push_enum(Ast *c, AstEnum a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.repr + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.members.len * (sizeof(a.members.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.members.len + 0, sizeof(int32_t));
    memcpy(extra, a.members.ptr, a.members.len * sizeof(a.members.ptr[0]));
    extra += a.members.len * (sizeof(a.members.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_ENUM, data);
}

AstId ast_push_struct(Ast *c, AstStruct a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.has_public_fields + 0, sizeof(int32_t));
    int32_t n = 2;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.type_params.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.fields.len + 0, sizeof(int32_t));
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_STRUCT, data);
}

AstId ast_push_union(Ast *c, AstUnion a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.has_public_fields + 0, sizeof(int32_t));
    int32_t n = 2;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.type_params.len + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.fields.len + 0, sizeof(int32_t));
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_UNION, data);
}

AstId ast_push_newtype(Ast *c, AstNewtype a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type_params.len + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_NEWTYPE, data);
}

AstId ast_push_const(Ast *c, AstConst a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.init + 0, sizeof(int32_t));
    return ast_new(c, AST_CONST, data);
}

AstId ast_push_extern_function(Ast *c, AstExternFunction a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.params.len + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.ret + 0, sizeof(int32_t));
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_EXTERN_FUNCTION, data);
}

AstId ast_push_extern_var(Ast *c, AstExternVar a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    return ast_new(c, AST_EXTERN_VAR, data);
}

AstId ast_push_param(Ast *c, AstParam a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    return ast_new(c, AST_PARAM, data);
}

AstId ast_push_let(Ast *c, AstTag tag, AstLet a) {
    switch (tag) {
        case AST_LET:
        case AST_MUT:
            break;
        default:
            abort();
    }
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.init + 0, sizeof(int32_t));
    return ast_new(c, tag, data);
}

AstId ast_push_if(Ast *c, AstIf a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.condition + 0, sizeof(int32_t));
    int32_t n = 2;
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.true_block + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.false_block + 0, sizeof(int32_t));
    return ast_new(c, AST_IF, data);
}

AstId ast_push_while(Ast *c, AstWhile a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.condition + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.block + 0, sizeof(int32_t));
    return ast_new(c, AST_WHILE, data);
}

AstId ast_push_for(Ast *c, AstFor a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.init + 0, sizeof(int32_t));
    int32_t n = 3;
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.condition + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.next + 0, sizeof(int32_t));
    memcpy(extra++, (int32_t *) &a.block + 0, sizeof(int32_t));
    return ast_new(c, AST_FOR, data);
}

AstId ast_push_switch(Ast *c, AstSwitch a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.condition + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.branches.len + 0, sizeof(int32_t));
    memcpy(extra, a.branches.ptr, a.branches.len * sizeof(a.branches.ptr[0]));
    extra += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_SWITCH, data);
}

AstId ast_push_switch_case(Ast *c, AstSwitchCase a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.pattern + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    return ast_new(c, AST_SWITCH_CASE, data);
}

AstId ast_push_break(Ast *c, AstBreak a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    return ast_new(c, AST_BREAK, data);
}

AstId ast_push_continue(Ast *c, AstContinue a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    return ast_new(c, AST_CONTINUE, data);
}

AstId ast_push_return(Ast *c, AstReturn a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.value + 0, sizeof(int32_t));
    return ast_new(c, AST_RETURN, data);
}

AstId ast_push_array_type(Ast *c, AstArrayType a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.index + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.elem + 0, sizeof(int32_t));
    return ast_new(c, AST_ARRAY_TYPE, data);
}

AstId ast_push_array_type_sugar(Ast *c, AstArrayTypeSugar a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.length + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.elem + 0, sizeof(int32_t));
    return ast_new(c, AST_ARRAY_TYPE_SUGAR, data);
}

AstId ast_push_unary(Ast *c, AstTag tag, AstUnary a) {
    switch (tag) {
        case AST_PTR_TYPE:
        case AST_MUT_PTR_TYPE:
        case AST_SLICE_TYPE:
        case AST_MUT_SLICE_TYPE:
        case AST_PLUS:
        case AST_MINUS:
        case AST_NOT:
        case AST_ADDRESS:
        case AST_DEREF:
            break;
        default:
            abort();
    }
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.a + 0, sizeof(int32_t));
    return ast_new(c, tag, data);
}

AstId ast_push_binary(Ast *c, AstTag tag, AstBinary a) {
    switch (tag) {
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MOD:
        case AST_AND:
        case AST_OR:
        case AST_XOR:
        case AST_SHL:
        case AST_SHR:
        case AST_LOGIC_AND:
        case AST_LOGIC_OR:
        case AST_EQ:
        case AST_NE:
        case AST_LT:
        case AST_GT:
        case AST_LE:
        case AST_GE:
        case AST_ASSIGN:
        case AST_ASSIGN_ADD:
        case AST_ASSIGN_SUB:
        case AST_ASSIGN_MUL:
        case AST_ASSIGN_DIV:
        case AST_ASSIGN_MOD:
        case AST_ASSIGN_AND:
        case AST_ASSIGN_OR:
        case AST_ASSIGN_XOR:
            break;
        default:
            abort();
    }
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.a + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.b + 0, sizeof(int32_t));
    return ast_new(c, tag, data);
}

AstId ast_push_function_type(Ast *c, AstFunctionType a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.params.len + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.ret + 0, sizeof(int32_t));
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_FUNCTION_TYPE, data);
}

AstId ast_push_type_hint(Ast *c, AstTypeHint a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.type + 0, sizeof(int32_t));
    memcpy(&data.c, (int32_t *) &a.value + 0, sizeof(int32_t));
    return ast_new(c, AST_TYPE_HINT, data);
}

AstId ast_push_call(Ast *c, AstTag tag, AstCall a) {
    switch (tag) {
        case AST_CALL:
        case AST_INDEX:
        case AST_SLICE:
            break;
        default:
            abort();
    }
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.a + 0, sizeof(int32_t));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra++, (int32_t *) &a.args.len + 0, sizeof(int32_t));
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return ast_new(c, tag, data);
}

AstId ast_push_access(Ast *c, AstAccess a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.s + 0, sizeof(int32_t));
    return ast_new(c, AST_ACCESS, data);
}

AstId ast_push_inferred_access(Ast *c, AstInferredAccess a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    return ast_new(c, AST_INFERRED_ACCESS, data);
}

AstId ast_push_list(Ast *c, AstList a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.elems.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.elems.len * (sizeof(a.elems.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, a.elems.ptr, a.elems.len * sizeof(a.elems.ptr[0]));
    extra += a.elems.len * (sizeof(a.elems.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_LIST, data);
}

AstId ast_push_map_entry(Ast *c, AstMapEntry a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.value + 0, sizeof(int32_t));
    return ast_new(c, AST_MAP_ENTRY, data);
}

AstId ast_push_map(Ast *c, AstMap a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.entries.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.entries.len * (sizeof(a.entries.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, a.entries.ptr, a.entries.len * sizeof(a.entries.ptr[0]));
    extra += a.entries.len * (sizeof(a.entries.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_MAP, data);
}

AstId ast_push_block(Ast *c, AstBlock a) {
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    memcpy(&data.b, (int32_t *) &a.stmts.len + 0, sizeof(int32_t));
    int32_t n = 0;
    n += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, a.stmts.ptr, a.stmts.len * sizeof(a.stmts.ptr[0]));
    extra += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    return ast_new(c, AST_BLOCK, data);
}

AstId ast_push_leaf(Ast *c, AstTag tag, AstLeaf a) {
    switch (tag) {
        case AST_ID:
        case AST_INT:
        case AST_FLOAT:
        case AST_CHAR:
        case AST_STRING:
        case AST_TRUE:
        case AST_FALSE:
        case AST_NULL:
            break;
        default:
            abort();
    }
    AstData data;
    memcpy(&data.a, (int32_t *) &a.token + 0, sizeof(int32_t));
    return ast_new(c, tag, data);
}

AstRoot ast_get_root(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ROOT:
            break;
        default:
            abort();
    }
    AstRoot result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.defs.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.defs.ptr = (void *) extra;
    extra += result.defs.len * (sizeof(result.defs.ptr[0]) / sizeof(int32_t));
    return result;
}

AstImport ast_get_import(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_IMPORT:
            break;
        default:
            abort();
    }
    AstImport result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    return result;
}

AstPublic ast_get_public(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_PUBLIC:
            break;
        default:
            abort();
    }
    AstPublic result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.def + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstFunction ast_get_function(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_FUNCTION:
            break;
        default:
            abort();
    }
    AstFunction result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.type_params.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.params.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.ret + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.body + 0, extra++, sizeof(int32_t));
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstEnum ast_get_enum(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ENUM:
            break;
        default:
            abort();
    }
    AstEnum result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.repr + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.members.len + 0, extra++, sizeof(int32_t));
    result.members.ptr = (void *) extra;
    extra += result.members.len * (sizeof(result.members.ptr[0]) / sizeof(int32_t));
    return result;
}

AstStruct ast_get_struct(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_STRUCT:
            break;
        default:
            abort();
    }
    AstStruct result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.has_public_fields + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.type_params.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.fields.len + 0, extra++, sizeof(int32_t));
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

AstUnion ast_get_union(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_UNION:
            break;
        default:
            abort();
    }
    AstUnion result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.has_public_fields + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.type_params.len + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.fields.len + 0, extra++, sizeof(int32_t));
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

AstNewtype ast_get_newtype(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_NEWTYPE:
            break;
        default:
            abort();
    }
    AstNewtype result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.type_params.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.type + 0, extra++, sizeof(int32_t));
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstConst ast_get_const(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_CONST:
            break;
        default:
            abort();
    }
    AstConst result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.init + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstExternFunction ast_get_extern_function(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_EXTERN_FUNCTION:
            break;
        default:
            abort();
    }
    AstExternFunction result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.params.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.ret + 0, extra++, sizeof(int32_t));
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstExternVar ast_get_extern_var(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_EXTERN_VAR:
            break;
        default:
            abort();
    }
    AstExternVar result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstParam ast_get_param(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_PARAM:
            break;
        default:
            abort();
    }
    AstParam result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstLet ast_get_let(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_LET:
        case AST_MUT:
            break;
        default:
            abort();
    }
    AstLet result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.init + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstIf ast_get_if(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_IF:
            break;
        default:
            abort();
    }
    AstIf result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.condition + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.true_block + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.false_block + 0, extra++, sizeof(int32_t));
    return result;
}

AstWhile ast_get_while(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_WHILE:
            break;
        default:
            abort();
    }
    AstWhile result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.condition + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.block + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstFor ast_get_for(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_FOR:
            break;
        default:
            abort();
    }
    AstFor result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.init + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.condition + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.next + 0, extra++, sizeof(int32_t));
    memcpy((int32_t *) &result.block + 0, extra++, sizeof(int32_t));
    return result;
}

AstSwitch ast_get_switch(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_SWITCH:
            break;
        default:
            abort();
    }
    AstSwitch result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.condition + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.branches.len + 0, extra++, sizeof(int32_t));
    result.branches.ptr = (void *) extra;
    extra += result.branches.len * (sizeof(result.branches.ptr[0]) / sizeof(int32_t));
    return result;
}

AstSwitchCase ast_get_switch_case(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_SWITCH_CASE:
            break;
        default:
            abort();
    }
    AstSwitchCase result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.pattern + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstBreak ast_get_break(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_BREAK:
            break;
        default:
            abort();
    }
    AstBreak result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    return result;
}

AstContinue ast_get_continue(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_CONTINUE:
            break;
        default:
            abort();
    }
    AstContinue result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    return result;
}

AstReturn ast_get_return(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_RETURN:
            break;
        default:
            abort();
    }
    AstReturn result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstArrayType ast_get_array_type(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ARRAY_TYPE:
            break;
        default:
            abort();
    }
    AstArrayType result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.index + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.elem + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstArrayTypeSugar ast_get_array_type_sugar(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ARRAY_TYPE_SUGAR:
            break;
        default:
            abort();
    }
    AstArrayTypeSugar result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.length + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.elem + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstUnary ast_get_unary(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_PTR_TYPE:
        case AST_MUT_PTR_TYPE:
        case AST_SLICE_TYPE:
        case AST_MUT_SLICE_TYPE:
        case AST_PLUS:
        case AST_MINUS:
        case AST_NOT:
        case AST_ADDRESS:
        case AST_DEREF:
            break;
        default:
            abort();
    }
    AstUnary result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstBinary ast_get_binary(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ADD:
        case AST_SUB:
        case AST_MUL:
        case AST_DIV:
        case AST_MOD:
        case AST_AND:
        case AST_OR:
        case AST_XOR:
        case AST_SHL:
        case AST_SHR:
        case AST_LOGIC_AND:
        case AST_LOGIC_OR:
        case AST_EQ:
        case AST_NE:
        case AST_LT:
        case AST_GT:
        case AST_LE:
        case AST_GE:
        case AST_ASSIGN:
        case AST_ASSIGN_ADD:
        case AST_ASSIGN_SUB:
        case AST_ASSIGN_MUL:
        case AST_ASSIGN_DIV:
        case AST_ASSIGN_MOD:
        case AST_ASSIGN_AND:
        case AST_ASSIGN_OR:
        case AST_ASSIGN_XOR:
            break;
        default:
            abort();
    }
    AstBinary result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.b + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstFunctionType ast_get_function_type(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_FUNCTION_TYPE:
            break;
        default:
            abort();
    }
    AstFunctionType result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.params.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.ret + 0, extra++, sizeof(int32_t));
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstTypeHint ast_get_type_hint(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_TYPE_HINT:
            break;
        default:
            abort();
    }
    AstTypeHint result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.type + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &nth(c->nodes.data_table, a).c, sizeof(int32_t));
    return result;
}

AstCall ast_get_call(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_CALL:
        case AST_INDEX:
        case AST_SLICE:
            break;
        default:
            abort();
    }
    AstCall result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.a + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy((int32_t *) &result.args.len + 0, extra++, sizeof(int32_t));
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

AstAccess ast_get_access(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ACCESS:
            break;
        default:
            abort();
    }
    AstAccess result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.s + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstInferredAccess ast_get_inferred_access(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_INFERRED_ACCESS:
            break;
        default:
            abort();
    }
    AstInferredAccess result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    return result;
}

AstList ast_get_list(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_LIST:
            break;
        default:
            abort();
    }
    AstList result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.elems.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.elems.ptr = (void *) extra;
    extra += result.elems.len * (sizeof(result.elems.ptr[0]) / sizeof(int32_t));
    return result;
}

AstMapEntry ast_get_map_entry(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_MAP_ENTRY:
            break;
        default:
            abort();
    }
    AstMapEntry result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.value + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    return result;
}

AstMap ast_get_map(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_MAP:
            break;
        default:
            abort();
    }
    AstMap result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.entries.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.entries.ptr = (void *) extra;
    extra += result.entries.len * (sizeof(result.entries.ptr[0]) / sizeof(int32_t));
    return result;
}

AstBlock ast_get_block(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_BLOCK:
            break;
        default:
            abort();
    }
    AstBlock result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    memcpy((int32_t *) &result.stmts.len + 0, &nth(c->nodes.data_table, a).b, sizeof(int32_t));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.stmts.ptr = (void *) extra;
    extra += result.stmts.len * (sizeof(result.stmts.ptr[0]) / sizeof(int32_t));
    return result;
}

AstLeaf ast_get_leaf(Ast *c, AstId a) {
    switch (ast_get_tag(c, a)) {
        case AST_ID:
        case AST_INT:
        case AST_FLOAT:
        case AST_CHAR:
        case AST_STRING:
        case AST_TRUE:
        case AST_FALSE:
        case AST_NULL:
            break;
        default:
            abort();
    }
    AstLeaf result;
    memcpy((int32_t *) &result.token + 0, &nth(c->nodes.data_table, a).a, sizeof(int32_t));
    return result;
}
