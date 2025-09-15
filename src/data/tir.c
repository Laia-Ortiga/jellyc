#include "data/tir.h"

#include "adt.h"
#include "arena.h"
#include "util.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Types

typedef struct {
    TirTag tag;
    union {
        TirId unary;
        int64_t array_length;
        ArrayType array;
        struct {
            TirId elem;
            TirId ptr;
        } slice;
        FunctionType function;
        TaggedType tagged;
    };
} StructuralType;

static StructuralType get_type_from_id(TirContext ctx, TirId type) {
    TirTag tag = get_term_tag(ctx, type);
    switch (tag) {
        case TIR_ARRAY_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .array = get_array_type(ctx, type),
            };
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            int64_t length = get_array_length_type(ctx, type);
            return (StructuralType) {.tag = tag, .array_length = length};
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .unary = remove_any_pointer(ctx, type),
            };
        }
        case TIR_FUNCTION_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .function = get_function_type(ctx, type),
            };
        }
        case TIR_TAGGED_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .tagged = get_tagged_type(ctx, type),
            };
        }
        case TIR_LINEAR_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .unary = get_linear_elem_type(ctx, type),
            };
        }
        default: {
            return (StructuralType) {.tag = tag, .unary = type};
        }
    }
}

static bool type_eq(StructuralType a, StructuralType b) {
    if (a.tag != b.tag) {
        return false;
    }
    switch (a.tag) {
        case TIR_PRIMITIVE_TYPE:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: return false;

        case TIR_ARRAY_TYPE: {
            return a.array.index.id == b.array.index.id
                && a.array.elem.id == b.array.elem.id;
        }
        case TIR_ARRAY_LENGTH_TYPE: return a.array_length == b.array_length;

        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_LINEAR_TYPE: return a.unary.id == b.unary.id;

        case TIR_FUNCTION_TYPE: {
            if (a.function.param_count != b.function.param_count) {
                return false;
            }
            for (int32_t i = 0; i < a.function.param_count; i++) {
                if (a.function.params[i].id != b.function.params[i].id) {
                    return false;
                }
            }
            return a.function.ret.id == b.function.ret.id;
        }
        case TIR_TAGGED_TYPE: {
            if (a.tagged.name != b.tagged.name) {
                return false;
            }
            if (a.tagged.arg_count != b.tagged.arg_count) {
                return false;
            }
            for (int32_t i = 0; i < a.tagged.arg_count; i++) {
                if (a.tagged.args[i].id != b.tagged.args[i].id) {
                    return false;
                }
            }
            return true;
        }
        default: {
            abort();
        }
    }
}

static int32_t hash_type(TirContext ctx, StructuralType type) {
    int32_t result = 17;
    result = 31 * result + type.tag;
    switch (type.tag) {
        case TIR_ARRAY_TYPE: {
            result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.array.index));
            result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.array.elem));
            break;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            result = 31 * result + type.array_length;
            break;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_LINEAR_TYPE: {
            result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.unary));
            break;
        }
        case TIR_FUNCTION_TYPE: {
            result = 31 * result + type.function.param_count;
            for (int32_t i = 0; i < type.function.param_count; i++) {
                result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.function.params[i]));
            }
            result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.function.ret));
            break;
        }
        case TIR_TAGGED_TYPE: {
            result = 31 * result + type.tagged.name;
            result = 31 * result + type.tagged.arg_count;
            for (int32_t i = 0; i < type.tagged.arg_count; i++) {
                result = 31 * result + hash_type(ctx, get_type_from_id(ctx, type.tagged.args[i]));
            }
            break;
        }
        default: {
            result = 31 * result + type.unary.id;
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

static void termset_insert_entry(TermSet *set, TirId key, TirContext ctx) {
    int32_t index = hash_type(ctx, get_type_from_id(ctx, key)) & (set->capacity - 1);
    while (set->ptr[index].id) {
        index = (index + 1) & (set->capacity - 1);
    }
    set->ptr[index] = key;
}

static void termset_resize(TermSet *set, TirContext ctx) {
    TermSet new_set = termset_init(set->capacity * 2);
    new_set.count = set->count;
    for (int32_t i = 0; i < set->capacity; i++) {
        if (set->ptr[i].id) {
            termset_insert_entry(&new_set, set->ptr[i], ctx);
        }
    }
    free(set->ptr);
    *set = new_set;
}

static TirDependencies *ctx_deps(TirContext ctx) {
    return ctx.thread ? &ctx.thread->deps : ctx.global;
}

static TermList *ctx_terms(TirContext ctx) {
    return &ctx_deps(ctx)->terms;
}

static TirId new_structural_type(TirContext ctx, StructuralType descriptor) {
    TermSet *set = &ctx_terms(ctx)->set;
    if (set->capacity == 0) {
        *set = termset_init(64);
    } else if (set->count * 4 / set->capacity >= 3) {
        termset_resize(set, ctx);
    }

    int32_t hash = hash_type(ctx, descriptor);
    int32_t slot = hash & (set->capacity - 1);
    while (set->ptr[slot].id) {
        if (type_eq(get_type_from_id(ctx, set->ptr[slot]), descriptor)) {
            return set->ptr[slot];
        }
        slot = (slot + 1) & (set->capacity - 1);
    }

    if (ctx.thread) {
        TermSet *global_set = &ctx.global->terms.set;
        int32_t global_slot = hash & (global_set->capacity - 1);
        while (global_set->ptr[global_slot].id) {
            if (type_eq(get_type_from_id(ctx, global_set->ptr[global_slot]), descriptor)) {
                return global_set->ptr[global_slot];
            }
            global_slot = (global_slot + 1) & (global_set->capacity - 1);
        }
    }

    TirId type = {ctx.global->terms.terms.len + TERM_COUNT};
    if (ctx.thread) {
        type.id += ctx.thread->deps.terms.terms.len;
    }
    set->ptr[slot] = type;
    set->count++;

    TermList *types = ctx_terms(ctx);
    switch (descriptor.tag) {
        default: {
            abort();
        }
        case TIR_ARRAY_TYPE: {
            TermData data = {
                .a = descriptor.array.elem.id,
                .b = descriptor.array.index.id,
            };
            sum_vec_push(&types->terms, data, descriptor.tag);
            break;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            sum_vec_push(&types->terms, *(TermData *) &descriptor.array_length, descriptor.tag);
            break;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_LINEAR_TYPE: {
            TermData data = {
                .a = descriptor.unary.id,
            };
            sum_vec_push(&types->terms, data, descriptor.tag);
            break;
        }
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            TermData data = {
                .a = descriptor.slice.elem.id,
                .b = descriptor.slice.ptr.id,
            };
            sum_vec_push(&types->terms, data, descriptor.tag);
            break;
        }
        case TIR_FUNCTION_TYPE: {
            int32_t index = types->extra.len;
            vec_push(&types->extra, descriptor.function.ret.id);
            for (int32_t i = 0; i < descriptor.function.param_count; i++) {
                vec_push(&types->extra, descriptor.function.params[i].id);
            }
            TermData data = {
                .a = descriptor.function.param_count,
                .b = index,
            };
            sum_vec_push(&types->terms, data, descriptor.tag);
            break;
        }
        case TIR_TAGGED_TYPE: {
            int32_t index = types->extra.len;
            vec_push(&types->extra, descriptor.tagged.name);
            vec_push(&types->extra, descriptor.tagged.inner.id);
            for (int32_t i = 0; i < descriptor.tagged.arg_count; i++) {
                vec_push(&types->extra, descriptor.tagged.args[i].id);
            }
            TermData data = {
                .a = descriptor.tagged.arg_count,
                .b = index,
            };
            sum_vec_push(&types->terms, data, descriptor.tag);
            break;
        }
    }

    return type;
}

static TirId new_tir(TirContext ctx, TirTag tag, TermData data) {
    if (!ctx.thread) {
        TirId t = {ctx.global->terms.terms.len + TERM_COUNT};
        sum_vec_push(&ctx.global->terms.terms, data, tag);
        return t;
    }

    TirId t = {ctx.global->terms.terms.len + TERM_COUNT + ctx.thread->deps.terms.terms.len};
    sum_vec_push(&ctx.thread->deps.terms.terms, data, tag);
    return t;
}

static TirId new_term(TirContext ctx, TirTag tag, int32_t a, int32_t b) {
    TermData data = {.a = a, .b = b};
    return new_tir(ctx, tag, data);
}

TirId new_array_type(TirContext ctx, ArrayType *t) {
    return new_structural_type(ctx, (StructuralType) {
        .tag = TIR_ARRAY_TYPE,
        .array = *t,
    });
}

TirId new_array_length_type(TirContext ctx, int64_t length) {
    return new_structural_type(ctx, (StructuralType) {
        .tag = TIR_ARRAY_LENGTH_TYPE,
        .array_length = length,
    });
}

TirId new_ptr_type(TirContext ctx, TirTag tag, TirId elem) {
    return new_structural_type(ctx, (StructuralType) {
        .tag = tag,
        .unary = elem,
    });
}

TirId new_multiptr_type(TirContext ctx, TirTag tag, TirId elem) {
    TirTag ptr = tag == TIR_MUT_SLICE_TYPE ? TIR_MUT_PTR_TYPE : TIR_PTR_TYPE;
    TirId pointer = new_ptr_type(ctx, ptr, ptype(byte));
    return new_structural_type(ctx, (StructuralType) {
        .tag = tag,
        .slice = {elem, pointer},
    });
}

TirId new_function_type(TirContext ctx, FunctionType *t) {
    return new_structural_type(ctx, (StructuralType) {
        .tag = TIR_FUNCTION_TYPE,
        .function = *t,
    });
}

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t alignment;
    int32_t size;
    int32_t is_linear;
} StructTypeLayout;

typedef struct {
    int32_t tags;
    int32_t name;
} NewtypeLayout;

static void init_struct_layout(StructTypeLayout *layout, TirContext ctx, int32_t field_count, TirId const *fields, Target target) {
    int32_t alignment = 1;
    int64_t size = 0;
    for (int32_t i = 0; i < field_count; i++) {
        int32_t field_align = alignof_type(ctx, fields[i], target);
        size = (size + field_align - 1) / field_align * field_align;
        size += sizeof_type(ctx, fields[i], target);
        if (field_align > alignment) {
            alignment = field_align;
        }
    }
    layout->alignment = alignment;
    layout->size = size;
}

TirId new_struct_type(TirContext ctx, Target target, StructType *t) {
    TermList *types = ctx_terms(ctx);
    int32_t index = types->extra.len;
    int32_t *ptr = vec_grow(&types->extra, t->field_count + sizeof(StructTypeLayout) / sizeof(int32_t));
    StructTypeLayout *layout = (StructTypeLayout *) ptr;
    layout->scope = t->scope;
    layout->name = t->name;
    init_struct_layout(layout, ctx, t->field_count, t->fields, target);
    layout->is_linear = false;
    for (int32_t i = 0; i < t->field_count; i++) {
        ptr[i + sizeof(StructTypeLayout) / sizeof(int32_t)] = t->fields[i].id;
        if (!layout->is_linear && type_is_linear(ctx, t->fields[i])) {
            layout->is_linear = true;
        }
    }
    return new_term(ctx, TIR_STRUCT_TYPE, t->field_count, index);
}

TirId new_enum_type(TirContext ctx, EnumType *t) {
    TermList *types = ctx_terms(ctx);
    int32_t index = types->extra.len;
    vec_push(&types->extra, t->scope);
    vec_push(&types->extra, t->name);
    return new_term(ctx, TIR_ENUM_TYPE, t->repr.id, index);
}

TirId new_tagged_type(TirContext ctx, TaggedType *t) {
    return new_structural_type(ctx, (StructuralType) {
        .tag = TIR_TAGGED_TYPE,
        .tagged = *t,
    });
}

TirId new_linear_type(TirContext ctx, TirId elem) {
    if (get_term_tag(ctx, elem) == TIR_LINEAR_TYPE) {
        return elem;
    }
    return new_structural_type(ctx, (StructuralType) {
        .tag = TIR_LINEAR_TYPE,
        .unary = elem,
    });
}

TirId new_type_parameter(TirContext ctx, int32_t i, int32_t name) {
    return new_term(ctx, TIR_TYPE_PARAMETER, i, name);
}

typedef struct {
    TirDependencies *deps;
    int32_t index;
} TermIndex;

static TirDependencies *get_term_deps(TirContext ctx, TirId type) {
    if (type.id - TERM_COUNT < ctx.global->terms.terms.len) {
        return ctx.global;
    }
    return &ctx.thread->deps;
}

static TermIndex get_term_index(TirContext ctx, TirId type) {
    if (type.id - TERM_COUNT < ctx.global->terms.terms.len) {
        return (TermIndex) {ctx.global, type.id - TERM_COUNT};
    }
    return (TermIndex) {&ctx.thread->deps, type.id - TERM_COUNT - ctx.global->terms.terms.len};
}

TirTag get_term_tag(TirContext ctx, TirId type) {
    if (type.id < TERM_COUNT) {
        if (type.id == 0) {
            return TIR_ERROR;
        }
        if (type.id >= BUILTIN_TYPE_START && type.id < BUILTIN_TYPE_END) {
            return TIR_PRIMITIVE_TYPE;
        }
        if (type.id >= BUILTIN_MACRO_START && type.id < BUILTIN_MACRO_END) {
            return TIR_MACRO;
        }
        return TIR_MODULE;
    }
    TermIndex i = get_term_index(ctx, type);
    return i.deps->terms.terms.tags[i.index];
}

TermData const *get_term_data(TirContext ctx, TirId type) {
    if (type.id < TERM_COUNT) {
        return NULL;
    }
    TermIndex i = get_term_index(ctx, type);
    return &i.deps->terms.terms.datas[i.index];
}

int32_t get_term_extra(TirContext ctx, int32_t index) {
    return ctx.thread->deps.terms.extra.ptr[index];
}

static int32_t *get_type_extra(TirContext ctx, TirId type) {
    if (type.id < TERM_COUNT) {
        return NULL;
    }
    TermIndex i = get_term_index(ctx, type);
    return &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
}

TirId remove_any_pointer(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return (TirId) {get_term_data(ctx, type)->a};

        default: return null_tir;
    }
}

TirId remove_pointer(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return (TirId) {get_term_data(ctx, type)->a};

        default: return null_tir;
    }
}

TirId remove_slice(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return (TirId) {get_term_data(ctx, type)->a};

        default: return null_tir;
    }
}

TirId replace_slice_with_pointer(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_SLICE_TYPE: return new_ptr_type(ctx, TIR_PTR_TYPE, (TirId) {get_term_data(ctx, type)->a});
        case TIR_MUT_SLICE_TYPE: return new_ptr_type(ctx, TIR_MUT_PTR_TYPE, (TirId) {get_term_data(ctx, type)->a});
        default: return null_tir;
    }
}

TirId replace_pointer_with_slice(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PTR_TYPE: return new_ptr_type(ctx, TIR_SLICE_TYPE, (TirId) {get_term_data(ctx, type)->a});
        case TIR_MUT_PTR_TYPE: return new_ptr_type(ctx, TIR_MUT_SLICE_TYPE, (TirId) {get_term_data(ctx, type)->a});
        default: return null_tir;
    }
}

TirId remove_c_pointer_like(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_ARRAY_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return (TirId) {get_term_data(ctx, type)->a};

        default: return null_tir;
    }
}

TirId remove_array_like(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_ARRAY_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return (TirId) {get_term_data(ctx, type)->a};

        default: return null_tir;
    }
}

TirId remove_tags(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_TAGGED_TYPE) {
        return type;
    }

    return get_tagged_type(ctx, type).inner;
}

bool is_aggregate_type(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PRIMITIVE_TYPE:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE: return false;

        case TIR_ARRAY_TYPE:
        case TIR_STRUCT_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_TYPE_PARAMETER: return true;

        case TIR_TAGGED_TYPE: return is_aggregate_type(ctx, get_tagged_type(ctx, type).inner);
        case TIR_LINEAR_TYPE: return is_aggregate_type(ctx, get_linear_elem_type(ctx, type));

        default: return false;
    }
}

bool type_is_linear(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_ARRAY_TYPE: return type_is_linear(ctx, get_array_type(ctx, type).elem);
        case TIR_TAGGED_TYPE: return type_is_linear(ctx, get_tagged_type(ctx, type).inner);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(ctx, type))->is_linear;
        case TIR_LINEAR_TYPE: return true;
        default: return false;
    }
}

bool type_is_unknown_size(TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_ARRAY_TYPE: return type_is_unknown_size(ctx, get_array_type(ctx, type).elem);
        case TIR_TAGGED_TYPE: return type_is_unknown_size(ctx, get_tagged_type(ctx, type).inner);
        case TIR_LINEAR_TYPE: return type_is_unknown_size(ctx, get_linear_elem_type(ctx, type));
        case TIR_TYPE_PARAMETER: return true;
        default: return false;
    }
}

bool is_equality_type(TirContext ctx, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (a.id == TYPE_bool) {
        return true;
    }
    if (a.id == TYPE_byte) {
        return true;
    }
    switch (get_term_tag(ctx, a)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE: return true;

        default: return false;
    }
}

bool is_relative_type(TirContext ctx, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (a.id == TYPE_byte) {
        return true;
    }
    switch (get_term_tag(ctx, a)) {
        case TIR_ENUM_TYPE: return true;
        default: return false;
    }
}

bool int_fits_in_bytes(int64_t i, int bytes) {
    switch (bytes) {
        case 1: return i >= INT8_MIN && i <= INT8_MAX;
        case 2: return i >= INT16_MIN && i <= INT16_MAX;
        case 4: return i >= INT32_MIN && i <= INT32_MAX;
        case 8: return true;
        default: abort();
    }
}

static int64_t sizeof_primitive(TirId type, Target target) {
    switch ((PrimitiveTerm) type.id) {
        case TYPE_INVALID:
        case TYPE_VOID: return -1;

        case TYPE_i8:
        case TYPE_char:
        case TYPE_bool:
        case TYPE_byte: return 1;

        case TYPE_i16: return 2;

        case TYPE_i32:
        case TYPE_f32: return 4;

        case TYPE_i64:
        case TYPE_f64: return 8;

        case TYPE_isize: return sizeof_pointer(target);
        default: break;
    }
    abort();
}

bool int_fits_in_type(int64_t i, TirId type, Target target) {
    switch (type.id) {
        case TYPE_char:
        case TYPE_i8:
        case TYPE_i16:
        case TYPE_i32:
        case TYPE_i64:
        case TYPE_isize: return int_fits_in_bytes(i, sizeof_primitive(type, target));

        default: return false;
    }
}

TirId bigger_primitive_type(TirId a, TirId b, Target target) {
    return sizeof_primitive(a, target) > sizeof_primitive(b, target) ? a : b;
}

ArrayType get_array_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_ARRAY_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(ctx, type);
    ArrayType array = {
        .index = {data->b},
        .elem = {data->a},
    };
    return array;
}

int64_t get_array_length_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_ARRAY_LENGTH_TYPE) {
        abort();
    }

    return *(int64_t const *) get_term_data(ctx, type);
}

TirId get_linear_elem_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_LINEAR_TYPE) {
        abort();
    }

    return (TirId) {get_term_data(ctx, type)->a};
}

int32_t get_type_parameter_index(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_TYPE_PARAMETER) {
        abort();
    }

    return get_term_data(ctx, type)->a;
}

FunctionType get_function_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_FUNCTION_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    FunctionType function = {
        .param_count = data->a,
        .params = (TirId *) &extra[1],
        .ret = {extra[0]},
    };
    return function;
}

TirId get_function_type_param(TirContext ctx, TirId type, int32_t index) {
    if (get_term_tag(ctx, type) != TIR_FUNCTION_TYPE) {
        return null_tir;
    }

    FunctionType f = get_function_type(ctx, type);
    if (index >= f.param_count) {
        return null_tir;
    }
    return f.params[index];
}

StructType get_struct_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_STRUCT_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    StructTypeLayout *layout = (StructTypeLayout *) extra;
    StructType s = {
        .scope = layout->scope,
        .name = layout->name,
        .field_count = data->a,
        .fields = (TirId *) &extra[sizeof(StructTypeLayout) / sizeof(int32_t)],
    };
    return s;
}

TirId get_struct_type_field(TirContext ctx, TirId type, int32_t index) {
    if (get_term_tag(ctx, type) != TIR_STRUCT_TYPE) {
        return null_tir;
    }

    StructType s = get_struct_type(ctx, type);
    if (index >= s.field_count) {
        return null_tir;
    }
    return s.fields[index];
}

TirId get_any_struct_type_field(TirContext ctx, TirId type, int32_t index) {
    if (get_term_tag(ctx, type) == TIR_SLICE_TYPE || get_term_tag(ctx, type) == TIR_MUT_SLICE_TYPE) {
        switch (index) {
            case 0: return ptype(isize);
            case 1: return (TirId) {get_term_data(ctx, type)->b};
            default: return null_tir;
        }
    }

    if (get_term_tag(ctx, type) != TIR_STRUCT_TYPE) {
        return null_tir;
    }

    return get_struct_type_field(ctx, type, index);
}

EnumType get_enum_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_ENUM_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    EnumType e = {
        .scope = extra[0],
        .name = extra[1],
        .repr = {data->a},
    };
    return e;
}

TaggedType get_tagged_type(TirContext ctx, TirId type) {
    if (get_term_tag(ctx, type) != TIR_TAGGED_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    TaggedType t = {
        .name = extra[0],
        .inner = {extra[1]},
        .arg_count = data->a,
        .args = (TirId *) &extra[2],
    };
    return t;
}

TirId get_tagged_type_arg(TirContext ctx, TirId type, int32_t index) {
    if (get_term_tag(ctx, type) != TIR_TAGGED_TYPE) {
        return null_tir;
    }

    TaggedType t = get_tagged_type(ctx, type);
    if (index >= t.arg_count) {
        return null_tir;
    }
    return t.args[index];
}

GenericTerm get_generic_term(TirContext ctx, TirId term) {
    if (get_term_tag(ctx, term) != TIR_GENERIC) {
        return (GenericTerm) {
            .inner = term,
            .type_count = 0,
            .types = NULL,
        };
    }

    TermData const *data = get_term_data(ctx, term);
    int32_t *extra = get_type_extra(ctx, term);
    return (GenericTerm) {
        .inner = {data->a},
        .type_count = extra[0],
        .types = (TirId *) &extra[1],
    };
}

int32_t sizeof_pointer(Target target) {
    switch (target) {
        case TARGET_ISIZE_64: return 8;
        case TARGET_ISIZE_32: return 4;
    }
    abort();
}

int32_t alignof_type(TirContext ctx, TirId type, Target target) {
    switch (get_term_tag(ctx, type)) {
        default: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_INVALID:
                case TYPE_VOID: return -1;

                case TYPE_i8:
                case TYPE_char:
                case TYPE_bool:
                case TYPE_byte: return 1;

                case TYPE_i16: return 2;

                case TYPE_i32:
                case TYPE_f32: return 4;

                case TYPE_i64:
                case TYPE_f64: return 8;

                case TYPE_isize: return sizeof_pointer(target);

                default: break;
            }
            abort();
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return sizeof_pointer(target);

        case TIR_ARRAY_TYPE: return alignof_type(ctx, get_array_type(ctx, type).elem, target);
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(ctx, type))->alignment;
        case TIR_ENUM_TYPE: return alignof_type(ctx, get_enum_type(ctx, type).repr, target);
        case TIR_TAGGED_TYPE: return alignof_type(ctx, get_tagged_type(ctx, type).inner, target);
        case TIR_LINEAR_TYPE: return alignof_type(ctx, get_linear_elem_type(ctx, type), target);
        case TIR_TYPE_PARAMETER: return -1;
    }
}

int64_t sizeof_type(TirContext ctx, TirId type, Target target) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PRIMITIVE_TYPE: return sizeof_primitive(type, target);

        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE: return sizeof_pointer(target);

        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return 2 * sizeof_pointer(target);

        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(ctx, type);
            int64_t length = get_array_length_type(ctx, array.index);
            return length * sizeof_type(ctx, array.elem, target);
        }
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(ctx, type))->size;
        case TIR_ENUM_TYPE: return sizeof_type(ctx, get_enum_type(ctx, type).repr, target);
        case TIR_TAGGED_TYPE: return sizeof_type(ctx, get_tagged_type(ctx, type).inner, target);
        case TIR_LINEAR_TYPE: return sizeof_type(ctx, get_linear_elem_type(ctx, type), target);
        case TIR_TYPE_PARAMETER: return -1;
        default: {
            abort();
        }
    }
}

void print_type(FILE *file, TirContext ctx, TirId type) {
    switch (get_term_tag(ctx, type)) {
        case TIR_PRIMITIVE_TYPE: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_INVALID: fprintf(file, "{error}"); return;
                case TYPE_VOID: fprintf(file, "void"); return;

                #define TYPE(type) case TYPE_##type: fprintf(file, #type); return;
                #include "simple-types"

                default: break;
            }
            compiler_error("print_type: unknown primitive type");
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(ctx, type);
            int64_t length = get_array_length_type(ctx, array.index);
            fprintf(file, "[:%ld]", length);
            print_type(file, ctx, array.elem);
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            int64_t length = get_array_length_type(ctx, type);
            fprintf(file, "`ArrayLength(%ld)", length);
            return;
        }
        case TIR_PTR_TYPE: {
            fprintf(file, "*");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TIR_MUT_PTR_TYPE: {
            fprintf(file, "*mut ");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TIR_SLICE_TYPE: {
            fprintf(file, "@");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TIR_MUT_SLICE_TYPE: {
            fprintf(file, "@mut ");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType f = get_function_type(ctx, type);
            fprintf(file, "function (");
            for (int32_t j = 0; j < f.param_count; j++) {
                if (j != 0) {
                    fprintf(file, ", ");
                }
                TirId param_type = get_function_type_param(ctx, type, j);
                print_type(file, ctx, param_type);
            }
            fprintf(file, ")");
            if (f.ret.id != TYPE_VOID) {
                fprintf(file, " -> ");
                print_type(file, ctx, f.ret);
            }
            return;
        }
        case TIR_ENUM_TYPE: {
            fprintf(file, "%s", get_term_deps(ctx, type)->strtab.ptr + get_enum_type(ctx, type).name);
            return;
        }
        case TIR_TAGGED_TYPE: {
            TaggedType t = get_tagged_type(ctx, type);
            fprintf(file, "%s", get_term_deps(ctx, type)->strtab.ptr + t.name);

            if (t.arg_count) {
                fprintf(file, "[");
                for (int32_t j = 0; j < t.arg_count; j++) {
                    if (j != 0) {
                        fprintf(file, ", ");
                    }
                    TirId arg = get_tagged_type_arg(ctx, type, j);
                    print_type(file, ctx, arg);
                }
                fprintf(file, "]");
            }
            return;
        }
        case TIR_LINEAR_TYPE: {
            fprintf(file, "`Affine[");
            print_type(file, ctx, get_linear_elem_type(ctx, type));
            fprintf(file, "]");
            return;
        }
        case TIR_TYPE_PARAMETER: {
            char const *name = get_value_str(ctx, type);
            fputs(name, file);
            return;
        }
        default: {
            compiler_error("print_type: unknown type tag");
        }
    }
}

void debug_type(TirContext ctx, TirId type) {
    print_type(stderr, ctx, type);
    printf("\n");
}

// Type matching

static int match_types_single(TirContext ctx, TirId *results, TirId type, TypeMatcher *matcher) {
    if (!type.id) {
        return 0;
    }

    if (matcher->extra) {
        int32_t i = matcher->extra - 1;
        if (!results[i].id) {
            results[i] = type;
        } else if (results[i].id != type.id) {
            return 0;
        }
    }

    switch (matcher->match_type) {
        case TYPE_MATCH_T: {
            return 1;
        }
        case TYPE_MATCH_BYTE: {
            return type.id == TYPE_byte;
        }
        case TYPE_MATCH_ARRAY: {
            if (get_term_tag(ctx, type) == TIR_ARRAY_TYPE) {
                ArrayType array_type = get_array_type(ctx, type);
                return match_types_single(ctx, results, array_type.index, &matcher->inner[0])
                    && match_types_single(ctx, results, array_type.elem, &matcher->inner[1]);
            }
            return 0;
        }
        case TYPE_MATCH_ANY_POINTER: {
            TirId inner = remove_pointer(ctx, type);
            return match_types_single(ctx, results, inner, matcher->inner);
        }
        case TYPE_MATCH_ANY_SLICE: {
            TirId inner = remove_slice(ctx, type);
            return match_types_single(ctx, results, inner, matcher->inner);
        }
        case TYPE_MATCH_POINTER: {
            if (get_term_tag(ctx, type) == TIR_PTR_TYPE) {
                TirId inner = remove_pointer(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_SLICE: {
            if (get_term_tag(ctx, type) == TIR_SLICE_TYPE) {
                TirId inner = remove_slice(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_POINTER: {
            if (get_term_tag(ctx, type) == TIR_MUT_PTR_TYPE) {
                TirId inner = remove_pointer(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_SLICE: {
            if (get_term_tag(ctx, type) == TIR_MUT_SLICE_TYPE) {
                TirId inner = remove_slice(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_TAGGED: {
            if (get_term_tag(ctx, type) == TIR_TAGGED_TYPE) {
                TirId inner = remove_tags(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
    }
    return 0;
}

int match_types(TirContext ctx, TirId *results, int32_t count, TirId *types, TypeMatcher *matchers) {
    for (int32_t i = 0; i < count; i++) {
        if (!match_types_single(ctx, results, types[i], &matchers[i])) {
            return 0;
        }
    }
    return 1;
}

int match_type_parameters(TirContext ctx, TirId *results, TirId param, TirId arg) {
    if (param.id == arg.id) {
        return 1;
    }

    TirTag tag = get_term_tag(ctx, param);
    if (tag == TIR_TYPE_PARAMETER) {
        int32_t index = get_term_data(ctx, param)->a;
        if (!results[index].id) {
            results[index] = arg;
        } else if (results[index].id != arg.id) {
            return 0;
        }
        return 1;
    }
    if (tag != get_term_tag(ctx, arg)) {
        return 0;
    }
    switch (tag) {
        case TIR_PRIMITIVE_TYPE: {
            return 1;
        }
        case TIR_ARRAY_TYPE: {
            ArrayType param_array = get_array_type(ctx, param);
            ArrayType arg_array = get_array_type(ctx, arg);
            return match_type_parameters(ctx, results, param_array.index, arg_array.index)
                && match_type_parameters(ctx, results, param_array.elem, arg_array.elem);
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return 0;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_LINEAR_TYPE: {
            TirId param_elem = {get_term_data(ctx, param)->a};
            TirId arg_elem = {get_term_data(ctx, arg)->a};
            return match_type_parameters(ctx, results, param_elem, arg_elem);
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType param_f = get_function_type(ctx, param);
            FunctionType arg_f = get_function_type(ctx, arg);
            if (param_f.param_count != arg_f.param_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_f.param_count; i++) {
                if (!match_type_parameters(ctx, results, get_function_type_param(ctx, param, i), get_function_type_param(ctx, arg, i))) {
                    return 0;
                }
            }
            return match_type_parameters(ctx, results, param_f.ret, arg_f.ret);
        }
        case TIR_TAGGED_TYPE: {
            TaggedType param_t = get_tagged_type(ctx, param);
            TaggedType arg_t = get_tagged_type(ctx, arg);
            if (param_t.name != arg_t.name) {
                return 0;
            }
            if (param_t.arg_count != arg_t.arg_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_t.arg_count; i++) {
                if (!match_type_parameters(ctx, results, get_tagged_type_arg(ctx, param, i), get_tagged_type_arg(ctx, arg, i))) {
                    return 0;
                }
            }
            return 1;
        }
        case TIR_STRUCT_TYPE:
        case TIR_ENUM_TYPE:
        case TIR_TYPE_PARAMETER: {
            break;
        }
        default: {
            abort();
        }
    }
    return 0;
}

TirId replace_type_parameters(TirId generic, ReplaceTypeInfo *info) {
    switch (get_term_tag(info->ctx, generic)) {
        case TIR_PRIMITIVE_TYPE:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_ENUM_TYPE: {
            return generic;
        }
        case TIR_TYPE_PARAMETER: {
            int32_t index = get_term_data(info->ctx, generic)->a;
            return info->args[index];
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(info->ctx, generic);
            return new_array_type(info->ctx, &(ArrayType) {
                .index = replace_type_parameters(array.index, info),
                .elem = replace_type_parameters(array.elem, info),
            });
        }
        case TIR_PTR_TYPE: {
            TirId elem = {get_term_data(info->ctx, generic)->a};
            return new_ptr_type(info->ctx, TIR_PTR_TYPE, replace_type_parameters(elem, info));
        }
        case TIR_MUT_PTR_TYPE: {
            TirId elem = {get_term_data(info->ctx, generic)->a};
            return new_ptr_type(info->ctx, TIR_MUT_PTR_TYPE, replace_type_parameters(elem, info));
        }
        case TIR_SLICE_TYPE: {
            TirId elem = {get_term_data(info->ctx, generic)->a};
            return new_multiptr_type(info->ctx, TIR_SLICE_TYPE, replace_type_parameters(elem, info));
        }
        case TIR_MUT_SLICE_TYPE: {
            TirId elem = {get_term_data(info->ctx, generic)->a};
            return new_multiptr_type(info->ctx, TIR_MUT_SLICE_TYPE, replace_type_parameters(elem, info));
        }
        case TIR_LINEAR_TYPE: {
            TirId elem = {get_term_data(info->ctx, generic)->a};
            return new_linear_type(info->ctx, replace_type_parameters(elem, info));
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType f = get_function_type(info->ctx, generic);
            TirId *params = arena_alloc(&info->scratch, TirId, f.param_count);
            for (int32_t i = 0; i < f.param_count; i++) {
                params[i] = replace_type_parameters(get_function_type_param(info->ctx, generic, i), info);
            }
            return new_function_type(info->ctx, &(FunctionType) {
                .param_count = f.param_count,
                .params = params,
                .ret = replace_type_parameters(f.ret, info),
            });
        }
        case TIR_TAGGED_TYPE: {
            TaggedType t = get_tagged_type(info->ctx, generic);
            TirId *tags = arena_alloc(&info->scratch, TirId, t.arg_count);
            for (int32_t i = 0; i < t.arg_count; i++) {
                tags[i] = replace_type_parameters(get_tagged_type_arg(info->ctx, generic, i), info);
            }
            TirId inner = replace_type_parameters(t.inner, info);
            return new_tagged_type(info->ctx, &(TaggedType) {
                .name = t.name,
                .inner = inner,
                .arg_count = t.arg_count,
                .args = tags,
            });
        }
        case TIR_STRUCT_TYPE: {
            StructType t = get_struct_type(info->ctx, generic);
            TirId *fields = arena_alloc(&info->scratch, TirId, t.field_count);
            for (int32_t i = 0; i < t.field_count; i++) {
                fields[i] = replace_type_parameters(get_struct_type_field(info->ctx, generic, i), info);
            }
            return new_struct_type(info->ctx, info->target, &(StructType) {
                .name = t.name,
                .scope = t.scope,
                .field_count = t.field_count,
                .fields = t.fields,
            });
        }
        default: {
            abort();
        }
    }
}

// Values

TirId new_int_constant(TirContext ctx, TirId type, int64_t x) {
    TermList *terms = ctx_terms(ctx);
    uint32_t low, high;
    store_i64(x, &low, &high);
    vec_push(&terms->extra, low);
    vec_push(&terms->extra, high);
    return new_tir(ctx, TIR_CONST_INT, (TermData) {
        .a = type.id,
        .b = terms->extra.len - 2,
    });
}

TirId new_float_constant(TirContext ctx, TirId type, double x) {
    TermList *terms = ctx_terms(ctx);
    uint32_t low, high;
    store_f64(x, &low, &high);
    vec_push(&terms->extra, low);
    vec_push(&terms->extra, high);
    return new_tir(ctx, TIR_CONST_FLOAT, (TermData) {
        .a = type.id,
        .b = terms->extra.len - 2,
    });
}

TirId new_null_constant(TirContext ctx, TirId type) {
    return new_tir(ctx, TIR_CONST_NULL, (TermData) {
        .a = type.id,
    });
}

TirId new_string_constant(TirContext ctx, TirId type, int32_t s) {
    return new_tir(ctx, TIR_STRING, (TermData) {
        .a = type.id,
        .b = s,
    });
}

TirId new_function(TirContext ctx, TirId type, int32_t name) {
    return new_tir(ctx, TIR_FUNCTION, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_extern_function(TirContext ctx, TirId type, int32_t name) {
    return new_tir(ctx, TIR_EXTERN_FUNCTION, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_extern_var(TirContext ctx, TirId type, int32_t name) {
    return new_tir(ctx, TIR_EXTERN_VAR, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_variable(TirContext ctx, AstId node, TirId type, int32_t index, bool mutable) {
    return new_tir(
        ctx,
        mutable ? TIR_MUTABLE_VARIABLE : TIR_VARIABLE,
        (TermData) {
            .node = node,
            .a = type.id,
            .b = index,
        }
    );
}

TirId new_unary_tir(TirContext ctx, TirTag tag, AstId node, TirId type, TirId a) {
    return new_tir(ctx, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a.id,
    });
}

TirId new_binary_tir(TirContext ctx, TirTag tag, AstId node, TirId type, TirId a, TirId b) {
    return new_tir(ctx, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a.id,
        .c = b.id,
    });
}

TirId new_instr(TirContext ctx, TirTag tag, AstId node, TirId type, int32_t a, int32_t b) {
    return new_tir(ctx, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a,
        .c = b,
    });
}

TirId new_generic(
    TirContext ctx,
    TirId inner,
    int32_t type_count,
    TirId *types
) {
    TermList *terms = ctx_terms(ctx);
    int32_t extra = terms->extra.len;
    vec_push(&terms->extra, type_count);
    for (int32_t i = 0; i < type_count; i++) {
        vec_push(&terms->extra, types[i].id);
    }
    return new_term(ctx, TIR_GENERIC, inner.id, extra);
}

TirId get_value_type(TirContext ctx, TirId value) {
    if (!is_tir_value(get_term_tag(ctx, value))) {
        return null_tir;
    }

    return (TirId) {get_term_data(ctx, value)->a};
}

ValueCategory get_value_category(TirContext ctx, TirId value) {
    switch (get_term_tag(ctx, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL: return VALUE_TEMPORARY;

        case TIR_EXTERN_VAR:
        case TIR_STRING:
        case TIR_VARIABLE: return VALUE_PLACE;

        case TIR_MUTABLE_VARIABLE: return VALUE_MUTABLE_PLACE;

        case TIR_LET:
        case TIR_MUT:
        case TIR_IF:
        case TIR_LOOP:
        case TIR_BREAK:
        case TIR_CONTINUE:
        case TIR_RETURN:
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
        case TIR_SWITCH: return VALUE_TEMPORARY;

        case TIR_DEREF:
        case TIR_INDEX: {
            TirId operand = {get_term_data(ctx, value)->b};
            switch (get_term_tag(ctx, get_value_type(ctx, operand))) {
                case TIR_PTR_TYPE: return VALUE_PLACE;
                case TIR_MUT_PTR_TYPE: return VALUE_MUTABLE_PLACE;
                default: return get_value_category(ctx, operand);
            }
        }
        case TIR_ACCESS: {
            TirId operand = {get_term_data(ctx, value)->b};
            return get_value_category(ctx, operand);
        }
        case TIR_SLICE: return VALUE_MULTIVALUE;
        default: break;
    }
    return VALUE_INVALID;
}

char const *get_value_str(TirContext ctx, TirId value) {
    TermIndex i = get_term_index(ctx, value);
    return &i.deps->strtab.ptr[i.deps->terms.terms.datas[i.index].b];
}

int64_t get_value_int(TirContext ctx, TirId value) {
    TermIndex i = get_term_index(ctx, value);
    int32_t *p = &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
    return load_i64(p[0], p[1]);
}

double get_value_float(TirContext ctx, TirId value) {
    TermIndex i = get_term_index(ctx, value);
    int32_t *p = &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
    return load_f64(p[0], p[1]);
}
