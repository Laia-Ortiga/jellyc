#include "ast.h"

#include <stdlib.h>

AstId ast_push_root(Ast *c, AstRoot a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    int32_t n = 1;
    n += a.defs.len * (sizeof(a.defs.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.defs.len;
    memcpy(extra, a.defs.ptr, a.defs.len * sizeof(a.defs.ptr[0]));
    extra += a.defs.len * (sizeof(a.defs.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_ROOT, data);
}

AstId ast_push_import(Ast *c, AstImport a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    return new_ast(c, AST_IMPORT, data);
}

AstId ast_push_public(Ast *c, AstPublic a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.def, sizeof(a.def));
    return new_ast(c, AST_PUBLIC, data);
}

AstId ast_push_function(Ast *c, AstFunction a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.ret, sizeof(a.ret));
    int32_t n = 3;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.type_params.len;
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    *extra++ = a.params.len;
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    memcpy(extra, &a.body, sizeof(a.body));
    extra += sizeof(a.body) / sizeof(int32_t);
    return new_ast(c, AST_FUNCTION, data);
}

AstId ast_push_enum(Ast *c, AstEnum a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.repr, sizeof(a.repr));
    int32_t n = 1;
    n += a.members.len * (sizeof(a.members.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.members.len;
    memcpy(extra, a.members.ptr, a.members.len * sizeof(a.members.ptr[0]));
    extra += a.members.len * (sizeof(a.members.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_ENUM, data);
}

AstId ast_push_struct(Ast *c, AstStruct a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    int32_t n = 2;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    n += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.type_params.len;
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    *extra++ = a.fields.len;
    memcpy(extra, a.fields.ptr, a.fields.len * sizeof(a.fields.ptr[0]));
    extra += a.fields.len * (sizeof(a.fields.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_STRUCT, data);
}

AstId ast_push_newtype(Ast *c, AstNewtype a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.type, sizeof(a.type));
    int32_t n = 1;
    n += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.type_params.len;
    memcpy(extra, a.type_params.ptr, a.type_params.len * sizeof(a.type_params.ptr[0]));
    extra += a.type_params.len * (sizeof(a.type_params.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_NEWTYPE, data);
}

AstId ast_push_const(Ast *c, AstConst a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.init, sizeof(a.init));
    return new_ast(c, AST_CONST, data);
}

AstId ast_push_extern_function(Ast *c, AstExternFunction a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.ret, sizeof(a.ret));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.params.len;
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_EXTERN_FUNCTION, data);
}

AstId ast_push_extern_var(Ast *c, AstExternVar a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.type, sizeof(a.type));
    return new_ast(c, AST_EXTERN_VAR, data);
}

AstId ast_push_param(Ast *c, AstParam a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.type, sizeof(a.type));
    return new_ast(c, AST_PARAM, data);
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
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.init, sizeof(a.init));
    return new_ast(c, tag, data);
}

AstId ast_push_if(Ast *c, AstIf a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.condition, sizeof(a.condition));
    int32_t n = 2;
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, &a.true_block, sizeof(a.true_block));
    extra += sizeof(a.true_block) / sizeof(int32_t);
    memcpy(extra, &a.false_block, sizeof(a.false_block));
    extra += sizeof(a.false_block) / sizeof(int32_t);
    return new_ast(c, AST_IF, data);
}

AstId ast_push_while(Ast *c, AstWhile a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.condition, sizeof(a.condition));
    memcpy(&data.c, &a.block, sizeof(a.block));
    return new_ast(c, AST_WHILE, data);
}

AstId ast_push_for(Ast *c, AstFor a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.init, sizeof(a.init));
    int32_t n = 3;
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    memcpy(extra, &a.condition, sizeof(a.condition));
    extra += sizeof(a.condition) / sizeof(int32_t);
    memcpy(extra, &a.next, sizeof(a.next));
    extra += sizeof(a.next) / sizeof(int32_t);
    memcpy(extra, &a.block, sizeof(a.block));
    extra += sizeof(a.block) / sizeof(int32_t);
    return new_ast(c, AST_FOR, data);
}

AstId ast_push_switch(Ast *c, AstSwitch a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.condition, sizeof(a.condition));
    int32_t n = 1;
    n += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.branches.len;
    memcpy(extra, a.branches.ptr, a.branches.len * sizeof(a.branches.ptr[0]));
    extra += a.branches.len * (sizeof(a.branches.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_SWITCH, data);
}

AstId ast_push_switch_case(Ast *c, AstSwitchCase a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.pattern, sizeof(a.pattern));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_ast(c, AST_SWITCH_CASE, data);
}

AstId ast_push_break(Ast *c, AstBreak a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    return new_ast(c, AST_BREAK, data);
}

AstId ast_push_continue(Ast *c, AstContinue a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    return new_ast(c, AST_CONTINUE, data);
}

AstId ast_push_return(Ast *c, AstReturn a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.value, sizeof(a.value));
    return new_ast(c, AST_RETURN, data);
}

AstId ast_push_array_type(Ast *c, AstArrayType a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.index, sizeof(a.index));
    memcpy(&data.c, &a.elem, sizeof(a.elem));
    return new_ast(c, AST_ARRAY_TYPE, data);
}

AstId ast_push_array_type_sugar(Ast *c, AstArrayTypeSugar a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.length, sizeof(a.length));
    memcpy(&data.c, &a.elem, sizeof(a.elem));
    return new_ast(c, AST_ARRAY_TYPE_SUGAR, data);
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
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.a, sizeof(a.a));
    return new_ast(c, tag, data);
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
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.a, sizeof(a.a));
    memcpy(&data.c, &a.b, sizeof(a.b));
    return new_ast(c, tag, data);
}

AstId ast_push_function_type(Ast *c, AstFunctionType a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.ret, sizeof(a.ret));
    int32_t n = 1;
    n += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.params.len;
    memcpy(extra, a.params.ptr, a.params.len * sizeof(a.params.ptr[0]));
    extra += a.params.len * (sizeof(a.params.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_FUNCTION_TYPE, data);
}

AstId ast_push_type_hint(Ast *c, AstTypeHint a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.type, sizeof(a.type));
    memcpy(&data.c, &a.value, sizeof(a.value));
    return new_ast(c, AST_TYPE_HINT, data);
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
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.a, sizeof(a.a));
    int32_t n = 1;
    n += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.args.len;
    memcpy(extra, a.args.ptr, a.args.len * sizeof(a.args.ptr[0]));
    extra += a.args.len * (sizeof(a.args.ptr[0]) / sizeof(int32_t));
    return new_ast(c, tag, data);
}

AstId ast_push_access(Ast *c, AstAccess a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.s, sizeof(a.s));
    return new_ast(c, AST_ACCESS, data);
}

AstId ast_push_inferred_access(Ast *c, AstInferredAccess a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    return new_ast(c, AST_INFERRED_ACCESS, data);
}

AstId ast_push_list(Ast *c, AstList a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    int32_t n = 1;
    n += a.elems.len * (sizeof(a.elems.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.elems.len;
    memcpy(extra, a.elems.ptr, a.elems.len * sizeof(a.elems.ptr[0]));
    extra += a.elems.len * (sizeof(a.elems.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_LIST, data);
}

AstId ast_push_map_entry(Ast *c, AstMapEntry a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    memcpy(&data.b, &a.value, sizeof(a.value));
    return new_ast(c, AST_MAP_ENTRY, data);
}

AstId ast_push_map(Ast *c, AstMap a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    int32_t n = 1;
    n += a.entries.len * (sizeof(a.entries.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.entries.len;
    memcpy(extra, a.entries.ptr, a.entries.len * sizeof(a.entries.ptr[0]));
    extra += a.entries.len * (sizeof(a.entries.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_MAP, data);
}

AstId ast_push_block(Ast *c, AstBlock a) {
    AstData data;
    memcpy(&data.a, &a.token, sizeof(a.token));
    int32_t n = 1;
    n += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    data.c = c->extra.len;
    int32_t *extra = vec_grow(&c->extra, n);
    *extra++ = a.stmts.len;
    memcpy(extra, a.stmts.ptr, a.stmts.len * sizeof(a.stmts.ptr[0]));
    extra += a.stmts.len * (sizeof(a.stmts.ptr[0]) / sizeof(int32_t));
    return new_ast(c, AST_BLOCK, data);
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
    memcpy(&data.a, &a.token, sizeof(a.token));
    return new_ast(c, tag, data);
}

AstRoot ast_get_root(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_ROOT:
            break;
        default:
            abort();
    }
    AstRoot result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.defs.len = *extra++;
    result.defs.ptr = (void *) extra;
    extra += result.defs.len * (sizeof(result.defs.ptr[0]) / sizeof(int32_t));
    return result;
}

AstImport ast_get_import(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_IMPORT:
            break;
        default:
            abort();
    }
    AstImport result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    return result;
}

AstPublic ast_get_public(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_PUBLIC:
            break;
        default:
            abort();
    }
    AstPublic result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.def, &nth(c->nodes.data_table, a).b, sizeof(result.def));
    return result;
}

AstFunction ast_get_function(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_FUNCTION:
            break;
        default:
            abort();
    }
    AstFunction result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.ret, &nth(c->nodes.data_table, a).b, sizeof(result.ret));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.type_params.len = *extra++;
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    result.params.len = *extra++;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    memcpy(&result.body, extra, sizeof(result.body));
    extra += sizeof(result.body) / sizeof(int32_t);
    return result;
}

AstEnum ast_get_enum(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_ENUM:
            break;
        default:
            abort();
    }
    AstEnum result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.repr, &nth(c->nodes.data_table, a).b, sizeof(result.repr));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.members.len = *extra++;
    result.members.ptr = (void *) extra;
    extra += result.members.len * (sizeof(result.members.ptr[0]) / sizeof(int32_t));
    return result;
}

AstStruct ast_get_struct(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_STRUCT:
            break;
        default:
            abort();
    }
    AstStruct result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.type_params.len = *extra++;
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    result.fields.len = *extra++;
    result.fields.ptr = (void *) extra;
    extra += result.fields.len * (sizeof(result.fields.ptr[0]) / sizeof(int32_t));
    return result;
}

AstNewtype ast_get_newtype(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_NEWTYPE:
            break;
        default:
            abort();
    }
    AstNewtype result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.type, &nth(c->nodes.data_table, a).b, sizeof(result.type));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.type_params.len = *extra++;
    result.type_params.ptr = (void *) extra;
    extra += result.type_params.len * (sizeof(result.type_params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstConst ast_get_const(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_CONST:
            break;
        default:
            abort();
    }
    AstConst result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.init, &nth(c->nodes.data_table, a).b, sizeof(result.init));
    return result;
}

AstExternFunction ast_get_extern_function(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_EXTERN_FUNCTION:
            break;
        default:
            abort();
    }
    AstExternFunction result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.ret, &nth(c->nodes.data_table, a).b, sizeof(result.ret));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.params.len = *extra++;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstExternVar ast_get_extern_var(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_EXTERN_VAR:
            break;
        default:
            abort();
    }
    AstExternVar result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.type, &nth(c->nodes.data_table, a).b, sizeof(result.type));
    return result;
}

AstParam ast_get_param(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_PARAM:
            break;
        default:
            abort();
    }
    AstParam result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.type, &nth(c->nodes.data_table, a).b, sizeof(result.type));
    return result;
}

AstLet ast_get_let(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_LET:
        case AST_MUT:
            break;
        default:
            abort();
    }
    AstLet result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.init, &nth(c->nodes.data_table, a).b, sizeof(result.init));
    return result;
}

AstIf ast_get_if(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_IF:
            break;
        default:
            abort();
    }
    AstIf result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.condition, &nth(c->nodes.data_table, a).b, sizeof(result.condition));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy(&result.true_block, extra, sizeof(result.true_block));
    extra += sizeof(result.true_block) / sizeof(int32_t);
    memcpy(&result.false_block, extra, sizeof(result.false_block));
    extra += sizeof(result.false_block) / sizeof(int32_t);
    return result;
}

AstWhile ast_get_while(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_WHILE:
            break;
        default:
            abort();
    }
    AstWhile result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.condition, &nth(c->nodes.data_table, a).b, sizeof(result.condition));
    memcpy(&result.block, &nth(c->nodes.data_table, a).c, sizeof(result.block));
    return result;
}

AstFor ast_get_for(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_FOR:
            break;
        default:
            abort();
    }
    AstFor result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.init, &nth(c->nodes.data_table, a).b, sizeof(result.init));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    memcpy(&result.condition, extra, sizeof(result.condition));
    extra += sizeof(result.condition) / sizeof(int32_t);
    memcpy(&result.next, extra, sizeof(result.next));
    extra += sizeof(result.next) / sizeof(int32_t);
    memcpy(&result.block, extra, sizeof(result.block));
    extra += sizeof(result.block) / sizeof(int32_t);
    return result;
}

AstSwitch ast_get_switch(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_SWITCH:
            break;
        default:
            abort();
    }
    AstSwitch result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.condition, &nth(c->nodes.data_table, a).b, sizeof(result.condition));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.branches.len = *extra++;
    result.branches.ptr = (void *) extra;
    extra += result.branches.len * (sizeof(result.branches.ptr[0]) / sizeof(int32_t));
    return result;
}

AstSwitchCase ast_get_switch_case(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_SWITCH_CASE:
            break;
        default:
            abort();
    }
    AstSwitchCase result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.pattern, &nth(c->nodes.data_table, a).b, sizeof(result.pattern));
    memcpy(&result.value, &nth(c->nodes.data_table, a).c, sizeof(result.value));
    return result;
}

AstBreak ast_get_break(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_BREAK:
            break;
        default:
            abort();
    }
    AstBreak result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    return result;
}

AstContinue ast_get_continue(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_CONTINUE:
            break;
        default:
            abort();
    }
    AstContinue result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    return result;
}

AstReturn ast_get_return(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_RETURN:
            break;
        default:
            abort();
    }
    AstReturn result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.value, &nth(c->nodes.data_table, a).b, sizeof(result.value));
    return result;
}

AstArrayType ast_get_array_type(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_ARRAY_TYPE:
            break;
        default:
            abort();
    }
    AstArrayType result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.index, &nth(c->nodes.data_table, a).b, sizeof(result.index));
    memcpy(&result.elem, &nth(c->nodes.data_table, a).c, sizeof(result.elem));
    return result;
}

AstArrayTypeSugar ast_get_array_type_sugar(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_ARRAY_TYPE_SUGAR:
            break;
        default:
            abort();
    }
    AstArrayTypeSugar result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.length, &nth(c->nodes.data_table, a).b, sizeof(result.length));
    memcpy(&result.elem, &nth(c->nodes.data_table, a).c, sizeof(result.elem));
    return result;
}

AstUnary ast_get_unary(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
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
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.a, &nth(c->nodes.data_table, a).b, sizeof(result.a));
    return result;
}

AstBinary ast_get_binary(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
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
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.a, &nth(c->nodes.data_table, a).b, sizeof(result.a));
    memcpy(&result.b, &nth(c->nodes.data_table, a).c, sizeof(result.b));
    return result;
}

AstFunctionType ast_get_function_type(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_FUNCTION_TYPE:
            break;
        default:
            abort();
    }
    AstFunctionType result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.ret, &nth(c->nodes.data_table, a).b, sizeof(result.ret));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.params.len = *extra++;
    result.params.ptr = (void *) extra;
    extra += result.params.len * (sizeof(result.params.ptr[0]) / sizeof(int32_t));
    return result;
}

AstTypeHint ast_get_type_hint(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_TYPE_HINT:
            break;
        default:
            abort();
    }
    AstTypeHint result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.type, &nth(c->nodes.data_table, a).b, sizeof(result.type));
    memcpy(&result.value, &nth(c->nodes.data_table, a).c, sizeof(result.value));
    return result;
}

AstCall ast_get_call(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_CALL:
        case AST_INDEX:
        case AST_SLICE:
            break;
        default:
            abort();
    }
    AstCall result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.a, &nth(c->nodes.data_table, a).b, sizeof(result.a));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.args.len = *extra++;
    result.args.ptr = (void *) extra;
    extra += result.args.len * (sizeof(result.args.ptr[0]) / sizeof(int32_t));
    return result;
}

AstAccess ast_get_access(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_ACCESS:
            break;
        default:
            abort();
    }
    AstAccess result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.s, &nth(c->nodes.data_table, a).b, sizeof(result.s));
    return result;
}

AstInferredAccess ast_get_inferred_access(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_INFERRED_ACCESS:
            break;
        default:
            abort();
    }
    AstInferredAccess result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    return result;
}

AstList ast_get_list(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_LIST:
            break;
        default:
            abort();
    }
    AstList result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.elems.len = *extra++;
    result.elems.ptr = (void *) extra;
    extra += result.elems.len * (sizeof(result.elems.ptr[0]) / sizeof(int32_t));
    return result;
}

AstMapEntry ast_get_map_entry(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_MAP_ENTRY:
            break;
        default:
            abort();
    }
    AstMapEntry result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    memcpy(&result.value, &nth(c->nodes.data_table, a).b, sizeof(result.value));
    return result;
}

AstMap ast_get_map(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_MAP:
            break;
        default:
            abort();
    }
    AstMap result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.entries.len = *extra++;
    result.entries.ptr = (void *) extra;
    extra += result.entries.len * (sizeof(result.entries.ptr[0]) / sizeof(int32_t));
    return result;
}

AstBlock ast_get_block(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
        case AST_BLOCK:
            break;
        default:
            abort();
    }
    AstBlock result;
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    int32_t *extra = c->extra.ptr + nth(c->nodes.data_table, a).c;
    result.stmts.len = *extra++;
    result.stmts.ptr = (void *) extra;
    extra += result.stmts.len * (sizeof(result.stmts.ptr[0]) / sizeof(int32_t));
    return result;
}

AstLeaf ast_get_leaf(Ast *c, AstId a) {
    switch (get_ast_tag(c, a)) {
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
    memcpy(&result.token, &nth(c->nodes.data_table, a).a, sizeof(result.token));
    return result;
}
