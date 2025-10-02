static void print_ast_node(AstPrinter *p, AstId a) {
    switch (get_ast_tag(p->ast, a)) {
        case AST_ROOT: {
            printf("Root");
            break;
        }
        case AST_IMPORT: {
            AstImport t = ast_get_import(p->ast, a);
            p->depth++;
            printf("Import(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_PUBLIC: {
            AstPublic t = ast_get_public(p->ast, a);
            p->depth++;
            printf("Public(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("def: ");
            print_ast_node(p, t.def);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_FUNCTION: {
            AstFunction t = ast_get_function(p->ast, a);
            p->depth++;
            printf("Function(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("type_params: [\n");
            for (int32_t i = 0; i < t.type_params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.type_params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth++);
            printf("params: [\n");
            for (int32_t i = 0; i < t.params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("ret: ");
            print_ast_node(p, t.ret);
            printf(",\n");
            print_indent(p->depth);
            printf("body: ");
            print_ast_node(p, t.body);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ENUM: {
            AstEnum t = ast_get_enum(p->ast, a);
            p->depth++;
            printf("Enum(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("repr: ");
            print_ast_node(p, t.repr);
            printf(",\n");
            print_indent(p->depth++);
            printf("members: [\n");
            for (int32_t i = 0; i < t.members.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.members.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_STRUCT: {
            AstStruct t = ast_get_struct(p->ast, a);
            p->depth++;
            printf("Struct(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("type_params: [\n");
            for (int32_t i = 0; i < t.type_params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.type_params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth++);
            printf("fields: [\n");
            for (int32_t i = 0; i < t.fields.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.fields.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_NEWTYPE: {
            AstNewtype t = ast_get_newtype(p->ast, a);
            p->depth++;
            printf("Newtype(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("type_params: [\n");
            for (int32_t i = 0; i < t.type_params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.type_params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("type: ");
            print_ast_node(p, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_CONST: {
            AstConst t = ast_get_const(p->ast, a);
            p->depth++;
            printf("Const(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_ast_node(p, t.init);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_EXTERN_FUNCTION: {
            AstExternFunction t = ast_get_extern_function(p->ast, a);
            p->depth++;
            printf("ExternFunction(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("params: [\n");
            for (int32_t i = 0; i < t.params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("ret: ");
            print_ast_node(p, t.ret);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_EXTERN_VAR: {
            AstExternVar t = ast_get_extern_var(p->ast, a);
            p->depth++;
            printf("ExternVar(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("type: ");
            print_ast_node(p, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_PARAM: {
            AstParam t = ast_get_param(p->ast, a);
            p->depth++;
            printf("Param(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("type: ");
            print_ast_node(p, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LET: {
            AstLet t = ast_get_let(p->ast, a);
            p->depth++;
            printf("Let(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_ast_node(p, t.init);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MUT: {
            AstLet t = ast_get_let(p->ast, a);
            p->depth++;
            printf("Mut(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_ast_node(p, t.init);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_IF: {
            AstIf t = ast_get_if(p->ast, a);
            p->depth++;
            printf("If(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_ast_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth);
            printf("true_block: ");
            print_ast_node(p, t.true_block);
            printf(",\n");
            print_indent(p->depth);
            printf("false_block: ");
            print_ast_node(p, t.false_block);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_WHILE: {
            AstWhile t = ast_get_while(p->ast, a);
            p->depth++;
            printf("While(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_ast_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth);
            printf("block: ");
            print_ast_node(p, t.block);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_FOR: {
            AstFor t = ast_get_for(p->ast, a);
            p->depth++;
            printf("For(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_ast_node(p, t.init);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_ast_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth);
            printf("next: ");
            print_ast_node(p, t.next);
            printf(",\n");
            print_indent(p->depth);
            printf("block: ");
            print_ast_node(p, t.block);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SWITCH: {
            AstSwitch t = ast_get_switch(p->ast, a);
            p->depth++;
            printf("Switch(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_ast_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth++);
            printf("branches: [\n");
            for (int32_t i = 0; i < t.branches.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.branches.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SWITCH_CASE: {
            AstSwitchCase t = ast_get_switch_case(p->ast, a);
            p->depth++;
            printf("SwitchCase(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("pattern: ");
            print_ast_node(p, t.pattern);
            printf(",\n");
            print_indent(p->depth);
            printf("value: ");
            print_ast_node(p, t.value);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_BREAK: {
            AstBreak t = ast_get_break(p->ast, a);
            p->depth++;
            printf("Break(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_CONTINUE: {
            AstContinue t = ast_get_continue(p->ast, a);
            p->depth++;
            printf("Continue(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_RETURN: {
            AstReturn t = ast_get_return(p->ast, a);
            p->depth++;
            printf("Return(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("value: ");
            print_ast_node(p, t.value);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ARRAY_TYPE: {
            AstArrayType t = ast_get_array_type(p->ast, a);
            p->depth++;
            printf("ArrayType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("index: ");
            print_ast_node(p, t.index);
            printf(",\n");
            print_indent(p->depth);
            printf("elem: ");
            print_ast_node(p, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ARRAY_TYPE_SUGAR: {
            AstArrayTypeSugar t = ast_get_array_type_sugar(p->ast, a);
            p->depth++;
            printf("ArrayTypeSugar(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("length: ");
            print_ast_node(p, t.length);
            printf(",\n");
            print_indent(p->depth);
            printf("elem: ");
            print_ast_node(p, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_PTR_TYPE: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("PtrType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MUT_PTR_TYPE: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("MutPtrType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SLICE_TYPE: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("SliceType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MUT_SLICE_TYPE: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("MutSliceType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_PLUS: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("Plus(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MINUS: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("Minus(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_NOT: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("Not(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ADDRESS: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("Address(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_DEREF: {
            AstUnary t = ast_get_unary(p->ast, a);
            p->depth++;
            printf("Deref(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ADD: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Add(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SUB: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Sub(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MUL: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Mul(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_DIV: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Div(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MOD: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Mod(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_AND: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("And(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_OR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Or(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_XOR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Xor(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SHL: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Shl(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SHR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Shr(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LOGIC_AND: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("LogicAnd(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LOGIC_OR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("LogicOr(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_EQ: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Eq(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_NE: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Ne(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LT: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Lt(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_GT: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Gt(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LE: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Le(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_GE: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Ge(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("Assign(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_ADD: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignAdd(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_SUB: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignSub(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_MUL: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignMul(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_DIV: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignDiv(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_MOD: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignMod(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_AND: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignAnd(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_OR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignOr(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ASSIGN_XOR: {
            AstBinary t = ast_get_binary(p->ast, a);
            p->depth++;
            printf("AssignXor(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_ast_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_FUNCTION_TYPE: {
            AstFunctionType t = ast_get_function_type(p->ast, a);
            p->depth++;
            printf("FunctionType(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("params: [\n");
            for (int32_t i = 0; i < t.params.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("ret: ");
            print_ast_node(p, t.ret);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_TYPE_HINT: {
            AstTypeHint t = ast_get_type_hint(p->ast, a);
            p->depth++;
            printf("TypeHint(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("type: ");
            print_ast_node(p, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("value: ");
            print_ast_node(p, t.value);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_CALL: {
            AstCall t = ast_get_call(p->ast, a);
            p->depth++;
            printf("Call(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_INDEX: {
            AstCall t = ast_get_call(p->ast, a);
            p->depth++;
            printf("Index(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_SLICE: {
            AstCall t = ast_get_call(p->ast, a);
            p->depth++;
            printf("Slice(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_ast_node(p, t.a);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ACCESS: {
            AstAccess t = ast_get_access(p->ast, a);
            p->depth++;
            printf("Access(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("s: ");
            print_ast_node(p, t.s);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_INFERRED_ACCESS: {
            AstInferredAccess t = ast_get_inferred_access(p->ast, a);
            p->depth++;
            printf("InferredAccess(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_LIST: {
            AstList t = ast_get_list(p->ast, a);
            p->depth++;
            printf("List(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("elems: [\n");
            for (int32_t i = 0; i < t.elems.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.elems.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MAP_ENTRY: {
            AstMapEntry t = ast_get_map_entry(p->ast, a);
            p->depth++;
            printf("MapEntry(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth);
            printf("value: ");
            print_ast_node(p, t.value);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_MAP: {
            AstMap t = ast_get_map(p->ast, a);
            p->depth++;
            printf("Map(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("entries: [\n");
            for (int32_t i = 0; i < t.entries.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.entries.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_BLOCK: {
            AstBlock t = ast_get_block(p->ast, a);
            p->depth++;
            printf("Block(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(p->depth++);
            printf("stmts: [\n");
            for (int32_t i = 0; i < t.stmts.len; i++) {
                print_indent(p->depth);
                print_ast_node(p, t.stmts.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_ID: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("Id(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_INT: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("Int(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_FLOAT: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("Float(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_CHAR: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("Char(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_STRING: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("String(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_TRUE: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("True(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_FALSE: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("False(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case AST_NULL: {
            AstLeaf t = ast_get_leaf(p->ast, a);
            p->depth++;
            printf("Null(\n");
            print_indent(p->depth);
            printf("token: ");
            Lexer lexer0 = new_lexer(substring(p->source, t.token.index, p->source.len));
            Token token = next_token(&lexer0);
            String s = substring(lexer0.source, token.start.index, token.end.index);
            fwrite(s.ptr, 1, s.len, stdout);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
    }
}
