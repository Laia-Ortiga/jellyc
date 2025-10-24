static void print_tir_node(TirPrinter *p, TirId a) {
    switch (tir_get_tag(p->tir, a)) {
        case TIR_ERROR: {
            printf("Error\n");
            break;
        }
        case TIR_RESERVED: {
            printf("Reserved\n");
            break;
        }
        case TIR_GENERIC: {
            TirGeneric t = tir_get_generic(p->tir, a);
            p->depth++;
            printf("Generic(\n");
            print_indent(p->depth);
            printf("inner: ");
            print_tir_node(p, t.inner);
            printf(",\n");
            print_indent(p->depth++);
            printf("params: [\n");
            for (int32_t i = 0; i < t.params.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_BLOCK: {
            TirBlock t = tir_get_block(p->tir, a);
            p->depth++;
            printf("Block(\n");
            print_indent(p->depth++);
            printf("stmts: [\n");
            for (int32_t i = 0; i < t.stmts.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.stmts.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ARRAY_TYPE: {
            TirArrayType t = tir_get_array_type(p->tir, a);
            p->depth++;
            printf("ArrayType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(p->depth);
            printf("index: ");
            print_type(stdout, p->tir, t.index);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            TirArrayLengthType t = tir_get_array_length_type(p->tir, a);
            p->depth++;
            printf("ArrayLengthType(\n");
            print_indent(p->depth);
            printf("length: %ld,\n", t.length);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_PTR_TYPE: {
            TirPtrType t = tir_get_ptr_type(p->tir, a);
            p->depth++;
            printf("PtrType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MUT_PTR_TYPE: {
            TirPtrType t = tir_get_ptr_type(p->tir, a);
            p->depth++;
            printf("MutPtrType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SLICE_TYPE: {
            TirSliceType t = tir_get_slice_type(p->tir, a);
            p->depth++;
            printf("SliceType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MUT_SLICE_TYPE: {
            TirSliceType t = tir_get_slice_type(p->tir, a);
            p->depth++;
            printf("MutSliceType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_FUNCTION_TYPE: {
            TirFunctionType t = tir_get_function_type(p->tir, a);
            p->depth++;
            printf("FunctionType(\n");
            print_indent(p->depth);
            printf("ret: ");
            print_type(stdout, p->tir, t.ret);
            printf(",\n");
            print_indent(p->depth++);
            printf("params: [\n");
            for (int32_t i = 0; i < t.params.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.params.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_TAGGED_TYPE: {
            TirTaggedType t = tir_get_tagged_type(p->tir, a);
            p->depth++;
            printf("TaggedType(\n");
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(p->depth);
            printf("inner: ");
            print_type(stdout, p->tir, t.inner);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_STRUCT_TYPE: {
            TirStructType t = tir_get_struct_type(p->tir, a);
            p->depth++;
            printf("StructType(\n");
            print_indent(p->depth);
            printf("scope: %d,\n", t.scope);
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(p->depth);
            printf("has_public_fields: %d,\n", t.has_public_fields);
            print_indent(p->depth++);
            printf("fields: [\n");
            for (int32_t i = 0; i < t.fields.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.fields.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("is_affine: %d,\n", t.is_affine);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_UNION_TYPE: {
            TirUnionType t = tir_get_union_type(p->tir, a);
            p->depth++;
            printf("UnionType(\n");
            print_indent(p->depth);
            printf("scope: %d,\n", t.scope);
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(p->depth);
            printf("has_public_fields: %d,\n", t.has_public_fields);
            print_indent(p->depth++);
            printf("fields: [\n");
            for (int32_t i = 0; i < t.fields.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.fields.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth);
            printf("is_affine: %d,\n", t.is_affine);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ENUM_TYPE: {
            TirEnumType t = tir_get_enum_type(p->tir, a);
            p->depth++;
            printf("EnumType(\n");
            print_indent(p->depth);
            printf("scope: %d,\n", t.scope);
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(p->depth);
            printf("repr: ");
            print_type(stdout, p->tir, t.repr);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_AFFINE_TYPE: {
            TirAffineType t = tir_get_affine_type(p->tir, a);
            p->depth++;
            printf("AffineType(\n");
            print_indent(p->depth);
            printf("elem: ");
            print_type(stdout, p->tir, t.elem);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_TYPE_PARAMETER: {
            TirTypeParameter t = tir_get_type_parameter(p->tir, a);
            p->depth++;
            printf("TypeParameter(\n");
            print_indent(p->depth);
            printf("index: %d,\n", t.index);
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_FUNCTION: {
            TirFunction t = tir_get_function(p->tir, a);
            p->depth++;
            printf("Function(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_EXTERN_FUNCTION: {
            TirExternFunction t = tir_get_extern_function(p->tir, a);
            p->depth++;
            printf("ExternFunction(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_EXTERN_VAR: {
            TirExternVar t = tir_get_extern_var(p->tir, a);
            p->depth++;
            printf("ExternVar(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("name: %s,\n", tir_get_str(p->tir, t.name));
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_INT: {
            TirInt t = tir_get_int(p->tir, a);
            p->depth++;
            printf("Int(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("value: %ld,\n", t.value);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_FLOAT: {
            TirFloat t = tir_get_float(p->tir, a);
            p->depth++;
            printf("Float(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("value: %f,\n", t.value);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_NULL: {
            TirNull t = tir_get_null(p->tir, a);
            p->depth++;
            printf("Null(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_STRING: {
            TirString t = tir_get_string(p->tir, a);
            p->depth++;
            printf("String(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("value: %s,\n", tir_get_str(p->tir, t.value));
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_PARAMETER: {
            TirVariable t = tir_get_variable(p->tir, a);
            p->depth++;
            printf("Parameter(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("index: %d,\n", t.index);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_VARIABLE: {
            TirVariable t = tir_get_variable(p->tir, a);
            p->depth++;
            printf("Variable(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("index: %d,\n", t.index);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MUTABLE_VARIABLE: {
            TirVariable t = tir_get_variable(p->tir, a);
            p->depth++;
            printf("MutableVariable(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("index: %d,\n", t.index);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_LET: {
            TirLet t = tir_get_let(p->tir, a);
            p->depth++;
            printf("Let(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("var: ");
            print_tir_node(p, t.var);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_tir_node(p, t.init);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_PLUS: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("Plus(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MINUS: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("Minus(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_NOT: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("Not(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_DEREF: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("Deref(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ADDRESS: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("Address(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ADDRESS_OF_TEMPORARY: {
            TirUnary t = tir_get_unary(p->tir, a);
            p->depth++;
            printf("AddressOfTemporary(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ADD: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Add(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SUB: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Sub(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MUL: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Mul(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_DIV: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Div(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_MOD: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Mod(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_AND: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("And(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_OR: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Or(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_XOR: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Xor(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SHL: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Shl(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SHR: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Shr(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_EQ: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Eq(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_NE: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Ne(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_LT: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Lt(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_GT: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Gt(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_LE: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Le(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_GE: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Ge(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("Assign(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_ADD: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignAdd(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_SUB: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignSub(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_MUL: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignMul(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_DIV: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignDiv(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_MOD: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignMod(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_AND: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignAnd(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_OR: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignOr(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ASSIGN_XOR: {
            TirBinary t = tir_get_binary(p->tir, a);
            p->depth++;
            printf("AssignXor(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("b: ");
            print_tir_node(p, t.b);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_CAST: {
            TirCast t = tir_get_cast(p->tir, a);
            p->depth++;
            printf("Cast(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_CHECKED_CAST: {
            TirCast t = tir_get_cast(p->tir, a);
            p->depth++;
            printf("CheckedCast(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_UNSIGNED_CAST: {
            TirCast t = tir_get_cast(p->tir, a);
            p->depth++;
            printf("UnsignedCast(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ARRAY_TO_SLICE: {
            TirCast t = tir_get_cast(p->tir, a);
            p->depth++;
            printf("ArrayToSlice(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SIZE_OF: {
            TirSizeOf t = tir_get_size_of(p->tir, a);
            p->depth++;
            printf("SizeOf(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("operand_type: ");
            print_type(stdout, p->tir, t.operand_type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ALIGN_OF: {
            TirAlignOf t = tir_get_align_of(p->tir, a);
            p->depth++;
            printf("AlignOf(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("operand_type: ");
            print_type(stdout, p->tir, t.operand_type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_CALL: {
            TirCall t = tir_get_call(p->tir, a);
            p->depth++;
            printf("Call(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("f: ");
            print_tir_node(p, t.f);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_INDEX: {
            TirIndex t = tir_get_index(p->tir, a);
            p->depth++;
            printf("Index(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("index: ");
            print_tir_node(p, t.index);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SLICE: {
            TirSlice t = tir_get_slice(p->tir, a);
            p->depth++;
            printf("Slice(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("a: ");
            print_tir_node(p, t.a);
            printf(",\n");
            print_indent(p->depth);
            printf("low: ");
            print_tir_node(p, t.low);
            printf(",\n");
            print_indent(p->depth);
            printf("high: ");
            print_tir_node(p, t.high);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_ACCESS: {
            TirAccess t = tir_get_access(p->tir, a);
            p->depth++;
            printf("Access(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("s: ");
            print_tir_node(p, t.s);
            printf(",\n");
            print_indent(p->depth);
            printf("field: %d,\n", t.field);
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_NEW_STRUCT: {
            TirNewStruct t = tir_get_new_struct(p->tir, a);
            p->depth++;
            printf("NewStruct(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth++);
            printf("fields: [\n");
            for (int32_t i = 0; i < t.fields.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.fields.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_NEW_ARRAY: {
            TirNewArray t = tir_get_new_array(p->tir, a);
            p->depth++;
            printf("NewArray(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth++);
            printf("args: [\n");
            for (int32_t i = 0; i < t.args.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.args.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_IF: {
            TirIf t = tir_get_if(p->tir, a);
            p->depth++;
            printf("If(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_tir_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth++);
            printf("true_block: [\n");
            for (int32_t i = 0; i < t.true_block.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.true_block.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(p->depth++);
            printf("false_block: [\n");
            for (int32_t i = 0; i < t.false_block.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.false_block.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_SWITCH: {
            TirSwitch t = tir_get_switch(p->tir, a);
            p->depth++;
            printf("Switch(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_tir_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth++);
            printf("branches: [\n");
            for (int32_t i = 0; i < t.branches.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.branches.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_LOOP: {
            TirLoop t = tir_get_loop(p->tir, a);
            p->depth++;
            printf("Loop(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("init: ");
            print_tir_node(p, t.init);
            printf(",\n");
            print_indent(p->depth);
            printf("condition: ");
            print_tir_node(p, t.condition);
            printf(",\n");
            print_indent(p->depth);
            printf("next: ");
            print_tir_node(p, t.next);
            printf(",\n");
            print_indent(p->depth++);
            printf("block: [\n");
            for (int32_t i = 0; i < t.block.len; i++) {
                print_indent(p->depth);
                print_tir_node(p, t.block.ptr[i]);
                printf(",\n");
            }
            print_indent(--p->depth);
            printf("],\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_BREAK: {
            TirBreak t = tir_get_break(p->tir, a);
            p->depth++;
            printf("Break(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_CONTINUE: {
            TirContinue t = tir_get_continue(p->tir, a);
            p->depth++;
            printf("Continue(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
        case TIR_RETURN: {
            TirReturn t = tir_get_return(p->tir, a);
            p->depth++;
            printf("Return(\n");
            print_indent(p->depth);
            printf("type: ");
            print_type(stdout, p->tir, t.type);
            printf(",\n");
            print_indent(p->depth);
            printf("value: ");
            print_tir_node(p, t.value);
            printf(",\n");
            print_indent(--p->depth);
            printf(")");
            break;
        }
    }
}
