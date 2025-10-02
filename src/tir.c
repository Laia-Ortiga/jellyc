#include "tir.h"

#include "adt.h"
#include "fwd.h"
#include "type.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    TirTag tag;
    union {
        TirArrayType array;
        TirArrayLengthType array_length;
        TirPtrType ptr;
        TirSliceType slice;
        TirFunctionType function;
        TirTaggedType tagged;
        TirAffineType affine;
        TirId nominal;
    };
} StructuralType;

static StructuralType get_type_from_id(TirContext c, TirId type) {
    TirTag tag = get_tir_tag(c, type);
    switch (tag) {
        case TIR_ARRAY_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .array = tir_get_array_type(c, type),
            };
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .array_length = tir_get_array_length_type(c, type),
            };
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .ptr = tir_get_ptr_type(c, type),
            };
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .slice = tir_get_slice_type(c, type),
            };
        }
        case TIR_FUNCTION_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .function = tir_get_function_type(c, type),
            };
        }
        case TIR_TAGGED_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .tagged = tir_get_tagged_type(c, type),
            };
        }
        case TIR_AFFINE_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .affine = tir_get_affine_type(c, type),
            };
        }
        default: {
            return (StructuralType) {
                .tag = tag,
                .nominal = type,
            };
        }
    }
}

static bool type_eq(StructuralType a, StructuralType b) {
    if (a.tag != b.tag) {
        return false;
    }
    switch (a.tag) {
        case TIR_RESERVED:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: {
            return false;
        }
        case TIR_ARRAY_TYPE: {
            return a.array.index.id == b.array.index.id
                && a.array.elem.id == b.array.elem.id;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return a.array_length.length == b.array_length.length;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            return a.ptr.elem.id == b.ptr.elem.id;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return a.slice.elem.id == b.slice.elem.id;
        }
        case TIR_FUNCTION_TYPE: {
            if (a.function.params.len != b.function.params.len) {
                return false;
            }
            for (int32_t i = 0; i < a.function.params.len; i++) {
                if (a.function.params.ptr[i].id != b.function.params.ptr[i].id) {
                    return false;
                }
            }
            return a.function.ret.id == b.function.ret.id;
        }
        case TIR_TAGGED_TYPE: {
            if (a.tagged.name != b.tagged.name) {
                return false;
            }
            if (a.tagged.args.len != b.tagged.args.len) {
                return false;
            }
            for (int32_t i = 0; i < a.tagged.args.len; i++) {
                if (a.tagged.args.ptr[i].id != b.tagged.args.ptr[i].id) {
                    return false;
                }
            }
            return true;
        }
        case TIR_AFFINE_TYPE: {
            return a.affine.elem.id == b.affine.elem.id;
        }
        default: {
            abort();
        }
    }
}

static int32_t hash_type(TirContext c, StructuralType type) {
    int32_t result = 17;
    result = 31 * result + type.tag;
    switch (type.tag) {
        case TIR_ARRAY_TYPE: {
            result = 31 * result + hash_type(c, get_type_from_id(c, type.array.index));
            result = 31 * result + hash_type(c, get_type_from_id(c, type.array.elem));
            break;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            result = 31 * result + type.array_length.length;
            break;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            result = 31 * result + hash_type(c, get_type_from_id(c, type.ptr.elem));
            break;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            result = 31 * result + hash_type(c, get_type_from_id(c, type.slice.elem));
            break;
        }
        case TIR_FUNCTION_TYPE: {
            result = 31 * result + type.function.params.len;
            for (int32_t i = 0; i < type.function.params.len; i++) {
                result = 31 * result + hash_type(c, get_type_from_id(c, type.function.params.ptr[i]));
            }
            result = 31 * result + hash_type(c, get_type_from_id(c, type.function.ret));
            break;
        }
        case TIR_TAGGED_TYPE: {
            result = 31 * result + type.tagged.name;
            result = 31 * result + type.tagged.args.len;
            for (int32_t i = 0; i < type.tagged.args.len; i++) {
                result = 31 * result + hash_type(c, get_type_from_id(c, type.tagged.args.ptr[i]));
            }
            break;
        }
        case TIR_AFFINE_TYPE: {
            result = 31 * result + hash_type(c, get_type_from_id(c, type.affine.elem));
            break;
        }
        default: {
            result = 31 * result + type.nominal.id;
            break;
        }
    }
    return result;
}

static TermSet termset_init(int32_t capacity) {
    TirId *ptr = calloc(capacity, sizeof(*ptr));
    if (!ptr) {
        abort();
    }
    return (TermSet) {
        .capacity = capacity,
        .count = 0,
        .ptr = ptr,
    };
}

static void termset_insert_entry(TermSet *set, TirId key, TirContext c) {
    int32_t index = hash_type(c, get_type_from_id(c, key)) & (set->capacity - 1);
    while (set->ptr[index].id) {
        index = (index + 1) & (set->capacity - 1);
    }
    set->ptr[index] = key;
}

static void termset_resize(TermSet *set, TirContext c) {
    TermSet new_set = termset_init(set->capacity * 2);
    new_set.count = set->count;
    for (int32_t i = 0; i < set->capacity; i++) {
        if (set->ptr[i].id) {
            termset_insert_entry(&new_set, set->ptr[i], c);
        }
    }
    free(set->ptr);
    *set = new_set;
}

static TermList *ctx_terms(TirContext c) {
    return &tir_writer(c)->terms;
}

static TirId new_structural_type(TirContext c, StructuralType descriptor) {
    TermSet *set = &ctx_terms(c)->set;
    if (set->capacity == 0) {
        *set = termset_init(64);
    } else if (set->count * 4 / set->capacity >= 3) {
        termset_resize(set, c);
    }

    int32_t hash = hash_type(c, descriptor);
    int32_t slot = hash & (set->capacity - 1);
    while (set->ptr[slot].id) {
        if (type_eq(get_type_from_id(c, set->ptr[slot]), descriptor)) {
            return set->ptr[slot];
        }
        slot = (slot + 1) & (set->capacity - 1);
    }

    if (c.thread) {
        TermSet *global_set = &c.global->terms.set;
        int32_t global_slot = hash & (global_set->capacity - 1);
        while (global_set->ptr[global_slot].id) {
            if (type_eq(get_type_from_id(c, global_set->ptr[global_slot]), descriptor)) {
                return global_set->ptr[global_slot];
            }
            global_slot = (global_slot + 1) & (global_set->capacity - 1);
        }
    }

    TirId type;
    switch (descriptor.tag) {
        case TIR_ARRAY_TYPE: {
            type = tir_push(c, descriptor.array);
            break;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            type = tir_push(c, descriptor.array_length);
            break;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: {
            type = tir_push_tag(c, descriptor.tag, descriptor.ptr);
            break;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            type = tir_push_tag(c, descriptor.tag, descriptor.slice);
            break;
        }
        case TIR_FUNCTION_TYPE: {
            type = tir_push(c, descriptor.function);
            break;
        }
        case TIR_TAGGED_TYPE: {
            type = tir_push(c, descriptor.tagged);
            break;
        }
        case TIR_AFFINE_TYPE: {
            type = tir_push(c, descriptor.affine);
            break;
        }
        default: {
            abort();
        }
    }

    set->ptr[slot] = type;
    set->count++;
    return type;
}

TirId new_tir(TirContext c, TirTag tag, TirData data) {
    if (!c.thread) {
        TirId t = {c.global->terms.terms.len + TERM_COUNT};
        sum_vec_push(&c.global->terms.terms, data, tag);
        return t;
    }

    TirId t = {c.global->terms.terms.len + TERM_COUNT + c.thread->terms.terms.len};
    sum_vec_push(&c.thread->terms.terms, data, tag);
    return t;
}

TirId new_array_type(TirContext c, TirArrayType t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_ARRAY_TYPE,
        .array = t,
    });
}

TirId new_array_length_type(TirContext c, int64_t length) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_ARRAY_LENGTH_TYPE,
        .array_length = {length},
    });
}

TirId new_ptr_type(TirContext c, TirId elem) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_PTR_TYPE,
        .ptr = {elem},
    });
}

TirId new_mut_ptr_type(TirContext c, TirId elem) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_MUT_PTR_TYPE,
        .ptr = {elem},
    });
}

TirId new_slice_type(TirContext c, TirId elem) {
    TirId pointer = new_ptr_type(c, elem);
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_SLICE_TYPE,
        .slice = {elem, pointer},
    });
}

TirId new_mut_slice_type(TirContext c, TirId elem) {
    TirId pointer = new_mut_ptr_type(c, elem);
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_MUT_SLICE_TYPE,
        .slice = {elem, pointer},
    });
}

TirId new_function_type(TirContext c, TirFunctionType t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_FUNCTION_TYPE,
        .function = t,
    });
}

static void init_struct_type_cache(TirContext c, Target target, TirStructType *t) {
    int32_t alignment = 1;
    int64_t size = 0;
    bool is_affine = false;
    for (int32_t i = 0; i < t->fields.len; i++) {
        int32_t field_align = alignof_type(c, t->fields.ptr[i], target);
        size = (size + field_align - 1) / field_align * field_align;
        size += sizeof_type(c, t->fields.ptr[i], target);
        if (field_align > alignment) {
            alignment = field_align;
        }
        if (!is_affine && type_is_affine(c, t->fields.ptr[i])) {
            is_affine = true;
        }
    }
    t->alignment = alignment;
    t->size = size;
    t->is_affine = is_affine;
}

TirId new_struct_type(TirContext c, Target target, TirStructType t) {
    init_struct_type_cache(c, target, &t);
    return tir_push_struct_type(c, t);
}

TirId new_tagged_type(TirContext c, TirTaggedType t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_TAGGED_TYPE,
        .tagged = t,
    });
}

TirId new_affine_type(TirContext c, TirId elem) {
    if (get_tir_tag(c, elem) == TIR_AFFINE_TYPE) {
        return elem;
    }
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_AFFINE_TYPE,
        .affine = {elem},
    });
}

typedef struct {
    Tir *tir;
    int32_t index;
} TermIndex;

static TermIndex get_term_index(TirContext c, TirId term) {
    if (term.id - TERM_COUNT < c.global->terms.terms.len) {
        return (TermIndex) {
            c.global,
            term.id - TERM_COUNT,
        };
    }
    return (TermIndex) {
        c.thread,
        term.id - TERM_COUNT - c.global->terms.terms.len,
    };
}

TirTag get_tir_tag(TirContext c, TirId term) {
    if (term.id < TERM_COUNT) {
        return term.id == RESERVED_ERROR ? TIR_ERROR : TIR_RESERVED;
    }
    TermIndex i = get_term_index(c, term);
    return i.tir->terms.terms.tags[i.index];
}

TirData const *get_term_data(TirContext c, TirId term) {
    if (term.id < TERM_COUNT) {
        return NULL;
    }
    TermIndex i = get_term_index(c, term);
    return &i.tir->terms.terms.datas[i.index];
}

int32_t get_term_extra(Tir *c, int32_t index) {
    return c->terms.extra.ptr[index];
}

TirId tir_block_get_last(TirBlock const *block) {
    assert(block->stmts.len > 0);
    return block->stmts.ptr[block->stmts.len - 1];
}

TirCategory get_term_category(TirContext c, TirId term) {
    TirTag tag = get_tir_tag(c, term);
    switch (tag) {
        case TIR_RESERVED: {
            if (term.id == 0) {
                return TIRCAT_ERROR;
            }
            if (term.id >= BUILTIN_TYPE_START && term.id < BUILTIN_TYPE_END) {
                return TIRCAT_TYPE;
            }
            if (term.id >= BUILTIN_MACRO_START && term.id < BUILTIN_MACRO_END) {
                return TIRCAT_MACRO;
            }
            return TIRCAT_MODULE;
        }
        case TIR_GENERIC: {
            return TIRCAT_OTHER;
        }
        case TIR_BLOCK: {
            TirBlock block = tir_get_block(c, term);
            if (block.stmts.len == 0) {
                return TIRCAT_OTHER;
            }
            TirId last = tir_block_get_last(&block);
            return get_term_category(c, last);
        }
        default: {
            if (tag >= TIR_TYPE_START && tag < TIR_TYPE_END) {
                return TIRCAT_TYPE;
            }
            if (tag >= TIR_VALUE_START && tag < TIR_VALUE_END) {
                return TIRCAT_VALUE;
            }
            return TIRCAT_ERROR;
        }
    }
}

TirId get_function_type_param(TirContext c, TirId type, int32_t index) {
    if (get_tir_tag(c, type) != TIR_FUNCTION_TYPE) {
        return error_term;
    }

    TirFunctionType f = tir_get_function_type(c, type);
    if (index >= f.params.len) {
        return error_term;
    }
    return f.params.ptr[index];
}

TirId get_struct_type_field(TirContext c, TirId type, int32_t index) {
    type = remove_tags(c, type);
    TirTag tag = get_tir_tag(c, type);

    if (tag == TIR_SLICE_TYPE || tag == TIR_MUT_SLICE_TYPE) {
        switch (index) {
            case 0: return ptype(isize);
            case 1: return tir_get_slice_type(c, type).cached_ptr;
            default: return error_term;
        }
    }

    if (tag != TIR_STRUCT_TYPE) {
        return error_term;
    }

    TirStructType s = tir_get_struct_type(c, type);
    if (index >= s.fields.len) {
        return error_term;
    }
    return s.fields.ptr[index];
}

TirId get_tagged_type_arg(TirContext c, TirId type, int32_t index) {
    if (get_tir_tag(c, type) != TIR_TAGGED_TYPE) {
        return error_term;
    }

    TirTaggedType t = tir_get_tagged_type(c, type);
    if (index >= t.args.len) {
        return error_term;
    }
    return t.args.ptr[index];
}

TirGeneric as_generic_term(TirContext c, TirId term) {
    if (get_tir_tag(c, term) != TIR_GENERIC) {
        return (TirGeneric) {
            .inner = term,
            .params = {0},
        };
    }

    return tir_get_generic(c, term);
}

TirId get_value_type(TirContext c, TirId value) {
    if (get_term_category(c, value) != TIRCAT_VALUE) {
        return error_term;
    }

    if (get_tir_tag(c, value) == TIR_BLOCK) {
        TirBlock block = tir_get_block(c, value);
        if (block.stmts.len == 0) {
            return error_term;
        }
        TirId last = tir_block_get_last(&block);
        return get_value_type(c, last);
    }

    return (TirId) {get_term_data(c, value)->b};
}

ValueCategory get_value_category(TirContext c, TirId value) {
    switch (get_tir_tag(c, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_INT:
        case TIR_FLOAT:
        case TIR_NULL:
        case TIR_PARAMETER:
        case TIR_LET:
        case TIR_PLUS:
        case TIR_MINUS:
        case TIR_NOT:
        case TIR_ADDRESS:
        case TIR_ADDRESS_OF_TEMPORARY:
        case TIR_ADD:
        case TIR_SUB:
        case TIR_MUL:
        case TIR_DIV:
        case TIR_MOD:
        case TIR_AND:
        case TIR_OR:
        case TIR_XOR:
        case TIR_SHL:
        case TIR_SHR:
        case TIR_EQ:
        case TIR_NE:
        case TIR_LT:
        case TIR_GT:
        case TIR_LE:
        case TIR_GE:
        case TIR_ASSIGN:
        case TIR_ASSIGN_ADD:
        case TIR_ASSIGN_SUB:
        case TIR_ASSIGN_MUL:
        case TIR_ASSIGN_DIV:
        case TIR_ASSIGN_MOD:
        case TIR_ASSIGN_AND:
        case TIR_ASSIGN_OR:
        case TIR_ASSIGN_XOR:
        case TIR_ITOF:
        case TIR_ITRUNC:
        case TIR_SEXT:
        case TIR_ZEXT:
        case TIR_FTOI:
        case TIR_FTRUNC:
        case TIR_FEXT:
        case TIR_PTR_CAST:
        case TIR_NOP:
        case TIR_ARRAY_TO_SLICE:
        case TIR_CALL:
        case TIR_NEW_STRUCT:
        case TIR_NEW_ARRAY:
        case TIR_IF:
        case TIR_SWITCH:
        case TIR_LOOP:
        case TIR_BREAK:
        case TIR_CONTINUE:
        case TIR_RETURN: return VALUE_TEMPORARY;

        case TIR_EXTERN_VAR:
        case TIR_STRING:
        case TIR_VARIABLE:
        case TIR_MUTABLE_VARIABLE:
        case TIR_DEREF:
        case TIR_INDEX:
        case TIR_ACCESS: return VALUE_PLACE;

        case TIR_SLICE: return VALUE_SLICE;
        case TIR_BLOCK: {
            TirBlock block = tir_get_block(c, value);
            if (block.stmts.len == 0) {
                return VALUE_INVALID;
            }
            TirId last = tir_block_get_last(&block);
            return get_value_category(c, last);
        }
        default: break;
    }
    return VALUE_INVALID;
}

bool is_value_mutable(TirContext c, TirId value) {
    switch (get_tir_tag(c, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_INT:
        case TIR_FLOAT:
        case TIR_NULL:
        case TIR_PARAMETER:
        case TIR_LET:
        case TIR_PLUS:
        case TIR_MINUS:
        case TIR_NOT:
        case TIR_ADDRESS:
        case TIR_ADDRESS_OF_TEMPORARY:
        case TIR_ADD:
        case TIR_SUB:
        case TIR_MUL:
        case TIR_DIV:
        case TIR_MOD:
        case TIR_AND:
        case TIR_OR:
        case TIR_XOR:
        case TIR_SHL:
        case TIR_SHR:
        case TIR_EQ:
        case TIR_NE:
        case TIR_LT:
        case TIR_GT:
        case TIR_LE:
        case TIR_GE:
        case TIR_ASSIGN:
        case TIR_ASSIGN_ADD:
        case TIR_ASSIGN_SUB:
        case TIR_ASSIGN_MUL:
        case TIR_ASSIGN_DIV:
        case TIR_ASSIGN_MOD:
        case TIR_ASSIGN_AND:
        case TIR_ASSIGN_OR:
        case TIR_ASSIGN_XOR:
        case TIR_ITOF:
        case TIR_ITRUNC:
        case TIR_SEXT:
        case TIR_ZEXT:
        case TIR_FTOI:
        case TIR_FTRUNC:
        case TIR_FEXT:
        case TIR_PTR_CAST:
        case TIR_NOP:
        case TIR_ARRAY_TO_SLICE:
        case TIR_CALL:
        case TIR_NEW_STRUCT:
        case TIR_NEW_ARRAY:
        case TIR_IF:
        case TIR_SWITCH:
        case TIR_LOOP:
        case TIR_BREAK:
        case TIR_CONTINUE:
        case TIR_RETURN:
        case TIR_MUTABLE_VARIABLE: return true;

        case TIR_EXTERN_VAR:
        case TIR_STRING:
        case TIR_VARIABLE: return false;

        case TIR_DEREF: {
            TirId operand = tir_get_unary(c, value).a;
            switch (get_tir_tag(c, get_value_type(c, operand))) {
                case TIR_PTR_TYPE: return false;
                case TIR_MUT_PTR_TYPE: return true;
                default: break;
            }
            break;
        }
        case TIR_INDEX: {
            return is_value_mutable(c, tir_get_index(c, value).a);
        }
        case TIR_SLICE: {
            return is_value_mutable(c, tir_get_slice(c, value).a);
        }
        case TIR_ACCESS: {
            return is_value_mutable(c, tir_get_access(c, value).s);
        }
        case TIR_BLOCK: {
            TirBlock block = tir_get_block(c, value);
            if (block.stmts.len == 0) {
                return VALUE_INVALID;
            }
            TirId last = tir_block_get_last(&block);
            return is_value_mutable(c, last);
        }
        default: break;
    }
    return false;
}

char const *tir_get_str(TirContext c, int32_t s) {
    return s < 0 ? &c.thread->strtab.ptr[~s] : &c.global->strtab.ptr[s];
}

int32_t tir_push_str(TirContext c, String s) {
    Tir *tir = tir_writer(c);
    int32_t index = push_str(&tir->strtab, s);
    return c.thread ? ~index : index;
}

int32_t tir_push_cstr(TirContext c, String s) {
    Tir *tir = tir_writer(c);
    int32_t index = push_str(&tir->strtab, s);
    push_str(&tir->strtab, (String) {1, ""});
    return c.thread ? ~index : index;
}
