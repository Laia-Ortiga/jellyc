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

static TirId get_type_elem(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ARRAY_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_AFFINE_TYPE: return (TirId) {get_term_data(c, type)->a};

        default: return null_tir;
    }
}

static StructuralType get_type_from_id(TirContext c, TirId type) {
    TirTag tag = get_term_tag(c, type);
    switch (tag) {
        case TIR_ARRAY_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .array = get_array_type(c, type),
            };
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            int64_t length = get_array_length_type(c, type);
            return (StructuralType) {.tag = tag, .array_length = length};
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .unary = get_type_elem(c, type),
            };
        }
        case TIR_FUNCTION_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .function = get_function_type(c, type),
            };
        }
        case TIR_TAGGED_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .tagged = get_tagged_type(c, type),
            };
        }
        case TIR_AFFINE_TYPE: {
            return (StructuralType) {
                .tag = tag,
                .unary = get_affine_elem_type(c, type),
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
        case TIR_AFFINE_TYPE: return a.unary.id == b.unary.id;

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
            result = 31 * result + type.array_length;
            break;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_AFFINE_TYPE: {
            result = 31 * result + hash_type(c, get_type_from_id(c, type.unary));
            break;
        }
        case TIR_FUNCTION_TYPE: {
            result = 31 * result + type.function.param_count;
            for (int32_t i = 0; i < type.function.param_count; i++) {
                result = 31 * result + hash_type(c, get_type_from_id(c, type.function.params[i]));
            }
            result = 31 * result + hash_type(c, get_type_from_id(c, type.function.ret));
            break;
        }
        case TIR_TAGGED_TYPE: {
            result = 31 * result + type.tagged.name;
            result = 31 * result + type.tagged.arg_count;
            for (int32_t i = 0; i < type.tagged.arg_count; i++) {
                result = 31 * result + hash_type(c, get_type_from_id(c, type.tagged.args[i]));
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

static Tir *ctx_write(TirContext c) {
    return c.thread ? c.thread : c.global;
}

static TermList *ctx_terms(TirContext c) {
    return &ctx_write(c)->terms;
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

    TirId type = {c.global->terms.terms.len + TERM_COUNT};
    if (c.thread) {
        type.id += c.thread->terms.terms.len;
    }
    set->ptr[slot] = type;
    set->count++;

    TermList *types = ctx_terms(c);
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
        case TIR_AFFINE_TYPE: {
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

static TirId new_tir(TirContext c, TirTag tag, TermData data) {
    if (!c.thread) {
        TirId t = {c.global->terms.terms.len + TERM_COUNT};
        sum_vec_push(&c.global->terms.terms, data, tag);
        return t;
    }

    TirId t = {c.global->terms.terms.len + TERM_COUNT + c.thread->terms.terms.len};
    sum_vec_push(&c.thread->terms.terms, data, tag);
    return t;
}

static TirId new_term(TirContext c, TirTag tag, int32_t a, int32_t b) {
    TermData data = {.a = a, .b = b};
    return new_tir(c, tag, data);
}

TirId new_array_type(TirContext c, ArrayType *t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_ARRAY_TYPE,
        .array = *t,
    });
}

TirId new_array_length_type(TirContext c, int64_t length) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_ARRAY_LENGTH_TYPE,
        .array_length = length,
    });
}

TirId new_ptr_type(TirContext c, TirId elem) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_PTR_TYPE,
        .unary = elem,
    });
}

TirId new_mut_ptr_type(TirContext c, TirId elem) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_MUT_PTR_TYPE,
        .unary = elem,
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

TirId new_function_type(TirContext c, FunctionType *t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_FUNCTION_TYPE,
        .function = *t,
    });
}

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t alignment;
    int32_t size;
    int32_t is_affine;
} StructTypeLayout;

typedef struct {
    int32_t tags;
    int32_t name;
} NewtypeLayout;

static void init_struct_layout(StructTypeLayout *layout, TirContext c, int32_t field_count, TirId const *fields, Target target) {
    int32_t alignment = 1;
    int64_t size = 0;
    for (int32_t i = 0; i < field_count; i++) {
        int32_t field_align = alignof_type(c, fields[i], target);
        size = (size + field_align - 1) / field_align * field_align;
        size += sizeof_type(c, fields[i], target);
        if (field_align > alignment) {
            alignment = field_align;
        }
    }
    layout->alignment = alignment;
    layout->size = size;
}

TirId new_struct_type(TirContext c, Target target, StructType *t) {
    TermList *types = ctx_terms(c);
    int32_t index = types->extra.len;
    int32_t *ptr = vec_grow(&types->extra, t->field_count + sizeof(StructTypeLayout) / sizeof(int32_t));
    StructTypeLayout *layout = (StructTypeLayout *) ptr;
    layout->scope = t->scope;
    layout->name = t->name;
    init_struct_layout(layout, c, t->field_count, t->fields, target);
    layout->is_affine = false;
    for (int32_t i = 0; i < t->field_count; i++) {
        ptr[i + sizeof(StructTypeLayout) / sizeof(int32_t)] = t->fields[i].id;
        if (!layout->is_affine && type_is_affine(c, t->fields[i])) {
            layout->is_affine = true;
        }
    }
    return new_term(c, TIR_STRUCT_TYPE, t->field_count, index);
}

TirId new_enum_type(TirContext c, EnumType *t) {
    TermList *types = ctx_terms(c);
    int32_t index = types->extra.len;
    vec_push(&types->extra, t->scope);
    vec_push(&types->extra, t->name);
    return new_term(c, TIR_ENUM_TYPE, t->repr.id, index);
}

TirId new_tagged_type(TirContext c, TaggedType *t) {
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_TAGGED_TYPE,
        .tagged = *t,
    });
}

TirId new_affine_type(TirContext c, TirId elem) {
    if (get_term_tag(c, elem) == TIR_AFFINE_TYPE) {
        return elem;
    }
    return new_structural_type(c, (StructuralType) {
        .tag = TIR_AFFINE_TYPE,
        .unary = elem,
    });
}

TirId new_type_parameter(TirContext c, int32_t i, int32_t name) {
    return new_term(c, TIR_TYPE_PARAMETER, i, name);
}

typedef struct {
    Tir *deps;
    int32_t index;
} TermIndex;

static TermIndex get_term_index(TirContext c, TirId type) {
    if (type.id - TERM_COUNT < c.global->terms.terms.len) {
        return (TermIndex) {c.global, type.id - TERM_COUNT};
    }
    return (TermIndex) {c.thread, type.id - TERM_COUNT - c.global->terms.terms.len};
}

TirTag get_term_tag(TirContext c, TirId type) {
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
    TermIndex i = get_term_index(c, type);
    return i.deps->terms.terms.tags[i.index];
}

TermData const *get_term_data(TirContext c, TirId type) {
    if (type.id < TERM_COUNT) {
        return NULL;
    }
    TermIndex i = get_term_index(c, type);
    return &i.deps->terms.terms.datas[i.index];
}

int32_t get_term_extra(TirContext c, int32_t index) {
    return c.thread->terms.extra.ptr[index];
}

static int32_t *get_type_extra(TirContext c, TirId type) {
    if (type.id < TERM_COUNT) {
        return NULL;
    }
    TermIndex i = get_term_index(c, type);
    return &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
}

TirId remove_any_pointer(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return get_type_elem(c, type);

        default: return null_tir;
    }
}

TirId remove_pointer(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE: return get_type_elem(c, type);

        default: return null_tir;
    }
}

TirId remove_slice(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return get_type_elem(c, type);

        default: return null_tir;
    }
}

TirId replace_slice_with_pointer(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_SLICE_TYPE: return new_ptr_type(c, get_type_elem(c, type));
        case TIR_MUT_SLICE_TYPE: return new_mut_ptr_type(c, get_type_elem(c, type));
        default: return null_tir;
    }
}

TirId replace_pointer_with_slice(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_PTR_TYPE: return new_slice_type(c, get_type_elem(c, type));
        case TIR_MUT_PTR_TYPE: return new_mut_slice_type(c, get_type_elem(c, type));
        default: return null_tir;
    }
}

TirId remove_c_pointer_like(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ARRAY_TYPE:
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return get_type_elem(c, type);

        default: return null_tir;
    }
}

TirId remove_array_like(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ARRAY_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return get_type_elem(c, type);

        default: return null_tir;
    }
}

TirId remove_tags(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_TAGGED_TYPE) {
        return type;
    }

    return get_tagged_type(c, type).inner;
}

bool is_aggregate_type(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
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

        case TIR_TAGGED_TYPE: return is_aggregate_type(c, get_tagged_type(c, type).inner);
        case TIR_AFFINE_TYPE: return is_aggregate_type(c, get_affine_elem_type(c, type));

        default: return false;
    }
}

bool type_is_affine(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ARRAY_TYPE: return type_is_affine(c, get_array_type(c, type).elem);
        case TIR_TAGGED_TYPE: return type_is_affine(c, get_tagged_type(c, type).inner);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(c, type))->is_affine;
        case TIR_AFFINE_TYPE: return true;
        default: return false;
    }
}

bool type_is_unknown_size(TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ARRAY_TYPE: return type_is_unknown_size(c, get_array_type(c, type).elem);
        case TIR_TAGGED_TYPE: return type_is_unknown_size(c, get_tagged_type(c, type).inner);
        case TIR_AFFINE_TYPE: return type_is_unknown_size(c, get_affine_elem_type(c, type));
        case TIR_TYPE_PARAMETER: return true;
        default: return false;
    }
}

bool is_equality_type(TirContext c, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (a.id == TYPE_bool) {
        return true;
    }
    if (a.id == TYPE_byte) {
        return true;
    }
    switch (get_term_tag(c, a)) {
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE:
        case TIR_ENUM_TYPE: return true;

        default: return false;
    }
}

bool is_relative_type(TirContext c, TirId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    switch (get_term_tag(c, a)) {
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

ArrayType get_array_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_ARRAY_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(c, type);
    ArrayType array = {
        .index = {data->b},
        .elem = {data->a},
    };
    return array;
}

int64_t get_array_length_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_ARRAY_LENGTH_TYPE) {
        abort();
    }

    return *(int64_t const *) get_term_data(c, type);
}

TirId get_affine_elem_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_AFFINE_TYPE) {
        abort();
    }

    return get_type_elem(c, type);
}

int32_t get_type_parameter_index(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_TYPE_PARAMETER) {
        abort();
    }

    return get_term_data(c, type)->a;
}

FunctionType get_function_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_FUNCTION_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(c, type);
    int32_t *extra = get_type_extra(c, type);
    FunctionType function = {
        .param_count = data->a,
        .params = (TirId *) &extra[1],
        .ret = {extra[0]},
    };
    return function;
}

TirId get_function_type_param(TirContext c, TirId type, int32_t index) {
    if (get_term_tag(c, type) != TIR_FUNCTION_TYPE) {
        return null_tir;
    }

    FunctionType f = get_function_type(c, type);
    if (index >= f.param_count) {
        return null_tir;
    }
    return f.params[index];
}

StructType get_struct_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_STRUCT_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(c, type);
    int32_t *extra = get_type_extra(c, type);
    StructTypeLayout *layout = (StructTypeLayout *) extra;
    StructType s = {
        .scope = layout->scope,
        .name = layout->name,
        .field_count = data->a,
        .fields = (TirId *) &extra[sizeof(StructTypeLayout) / sizeof(int32_t)],
    };
    return s;
}

TirId get_struct_type_field(TirContext c, TirId type, int32_t index) {
    if (get_term_tag(c, type) != TIR_STRUCT_TYPE) {
        return null_tir;
    }

    StructType s = get_struct_type(c, type);
    if (index >= s.field_count) {
        return null_tir;
    }
    return s.fields[index];
}

TirId get_any_struct_type_field(TirContext c, TirId type, int32_t index) {
    type = remove_tags(c, type);
    TirTag tag = get_term_tag(c, type);

    if (tag == TIR_SLICE_TYPE || tag == TIR_MUT_SLICE_TYPE) {
        switch (index) {
            case 0: return ptype(isize);
            case 1: return (TirId) {get_term_data(c, type)->b};
            default: return null_tir;
        }
    }

    if (tag != TIR_STRUCT_TYPE) {
        return null_tir;
    }

    return get_struct_type_field(c, type, index);
}

EnumType get_enum_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_ENUM_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(c, type);
    int32_t *extra = get_type_extra(c, type);
    EnumType e = {
        .scope = extra[0],
        .name = extra[1],
        .repr = {data->a},
    };
    return e;
}

TaggedType get_tagged_type(TirContext c, TirId type) {
    if (get_term_tag(c, type) != TIR_TAGGED_TYPE) {
        abort();
    }

    TermData const *data = get_term_data(c, type);
    int32_t *extra = get_type_extra(c, type);
    TaggedType t = {
        .name = extra[0],
        .inner = {extra[1]},
        .arg_count = data->a,
        .args = (TirId *) &extra[2],
    };
    return t;
}

TirId get_tagged_type_arg(TirContext c, TirId type, int32_t index) {
    if (get_term_tag(c, type) != TIR_TAGGED_TYPE) {
        return null_tir;
    }

    TaggedType t = get_tagged_type(c, type);
    if (index >= t.arg_count) {
        return null_tir;
    }
    return t.args[index];
}

GenericTerm get_generic_term(TirContext c, TirId term) {
    if (get_term_tag(c, term) != TIR_GENERIC) {
        return (GenericTerm) {
            .inner = term,
            .type_count = 0,
            .types = NULL,
        };
    }

    TermData const *data = get_term_data(c, term);
    int32_t *extra = get_type_extra(c, term);
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

int32_t alignof_type(TirContext c, TirId type, Target target) {
    switch (get_term_tag(c, type)) {
        case TIR_ERROR: return -1;
        case TIR_TYPE_PARAMETER: return -1;

        default: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_VOID: return -1;

                case TYPE_i8:
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

        case TIR_ARRAY_TYPE: return alignof_type(c, get_array_type(c, type).elem, target);
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(c, type))->alignment;
        case TIR_ENUM_TYPE: return alignof_type(c, get_enum_type(c, type).repr, target);
        case TIR_TAGGED_TYPE: return alignof_type(c, get_tagged_type(c, type).inner, target);
        case TIR_AFFINE_TYPE: return alignof_type(c, get_affine_elem_type(c, type), target);
    }
}

int64_t sizeof_type(TirContext c, TirId type, Target target) {
    switch (get_term_tag(c, type)) {
        case TIR_ERROR: return -1;
        case TIR_TYPE_PARAMETER: return -1;

        case TIR_PRIMITIVE_TYPE: return sizeof_primitive(type, target);

        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_FUNCTION_TYPE: return sizeof_pointer(target);

        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE: return 2 * sizeof_pointer(target);

        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(c, type);
            int64_t length = get_array_length_type(c, array.index);
            return length * sizeof_type(c, array.elem, target);
        }
        case TIR_ARRAY_LENGTH_TYPE: return sizeof_pointer(target);
        case TIR_STRUCT_TYPE: return ((StructTypeLayout *) get_type_extra(c, type))->size;
        case TIR_ENUM_TYPE: return sizeof_type(c, get_enum_type(c, type).repr, target);
        case TIR_TAGGED_TYPE: return sizeof_type(c, get_tagged_type(c, type).inner, target);
        case TIR_AFFINE_TYPE: return sizeof_type(c, get_affine_elem_type(c, type), target);
        default: {
            abort();
        }
    }
}

void print_type(FILE *file, TirContext c, TirId type) {
    switch (get_term_tag(c, type)) {
        case TIR_ERROR: {
            fprintf(file, "{error}");
            return;
        }
        case TIR_PRIMITIVE_TYPE: {
            switch ((PrimitiveTerm) type.id) {
                case TYPE_VOID: fprintf(file, "void"); return;

                #define TYPE(type) case TYPE_##type: fprintf(file, #type); return;
                #include "simple-types"

                default: break;
            }
            compiler_error("print_type: unknown primitive type");
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(c, type);
            int64_t length = get_array_length_type(c, array.index);
            fprintf(file, "[:%ld]", length);
            print_type(file, c, array.elem);
            return;
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            int64_t length = get_array_length_type(c, type);
            fprintf(file, "`ArrayLength(%ld)", length);
            return;
        }
        case TIR_PTR_TYPE: {
            fprintf(file, "*");
            print_type(file, c, get_type_elem(c, type));
            return;
        }
        case TIR_MUT_PTR_TYPE: {
            fprintf(file, "*mut ");
            print_type(file, c, get_type_elem(c, type));
            return;
        }
        case TIR_SLICE_TYPE: {
            fprintf(file, "@");
            print_type(file, c, get_type_elem(c, type));
            return;
        }
        case TIR_MUT_SLICE_TYPE: {
            fprintf(file, "@mut ");
            print_type(file, c, get_type_elem(c, type));
            return;
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType f = get_function_type(c, type);
            fprintf(file, "function (");
            for (int32_t j = 0; j < f.param_count; j++) {
                if (j != 0) {
                    fprintf(file, ", ");
                }
                TirId param_type = get_function_type_param(c, type, j);
                print_type(file, c, param_type);
            }
            fprintf(file, ")");
            if (f.ret.id != TYPE_VOID) {
                fprintf(file, " -> ");
                print_type(file, c, f.ret);
            }
            return;
        }
        case TIR_ENUM_TYPE: {
            fprintf(file, "%s", tir_get_str(c, get_enum_type(c, type).name));
            return;
        }
        case TIR_TAGGED_TYPE: {
            TaggedType t = get_tagged_type(c, type);
            fprintf(file, "%s", tir_get_str(c, t.name));

            if (t.arg_count) {
                fprintf(file, "[");
                for (int32_t j = 0; j < t.arg_count; j++) {
                    if (j != 0) {
                        fprintf(file, ", ");
                    }
                    TirId arg = get_tagged_type_arg(c, type, j);
                    print_type(file, c, arg);
                }
                fprintf(file, "]");
            }
            return;
        }
        case TIR_AFFINE_TYPE: {
            fprintf(file, "`Affine[");
            print_type(file, c, get_affine_elem_type(c, type));
            fprintf(file, "]");
            return;
        }
        case TIR_TYPE_PARAMETER: {
            char const *name = get_value_str(c, type);
            fputs(name, file);
            return;
        }
        default: {
            compiler_error("print_type: unknown type tag");
        }
    }
}

void debug_type(TirContext c, TirId type) {
    print_type(stderr, c, type);
    printf("\n");
}

// Type matching

static int match_types_single(TirContext c, TirId *results, TirId type, TypeMatcher *matcher) {
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
            if (get_term_tag(c, type) == TIR_ARRAY_TYPE) {
                ArrayType array_type = get_array_type(c, type);
                return match_types_single(c, results, array_type.index, &matcher->inner[0])
                    && match_types_single(c, results, array_type.elem, &matcher->inner[1]);
            }
            return 0;
        }
        case TYPE_MATCH_ANY_POINTER: {
            TirId inner = remove_pointer(c, type);
            return match_types_single(c, results, inner, matcher->inner);
        }
        case TYPE_MATCH_ANY_SLICE: {
            TirId inner = remove_slice(c, type);
            return match_types_single(c, results, inner, matcher->inner);
        }
        case TYPE_MATCH_POINTER: {
            if (get_term_tag(c, type) == TIR_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_SLICE: {
            if (get_term_tag(c, type) == TIR_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_POINTER: {
            if (get_term_tag(c, type) == TIR_MUT_PTR_TYPE) {
                TirId inner = remove_pointer(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_SLICE: {
            if (get_term_tag(c, type) == TIR_MUT_SLICE_TYPE) {
                TirId inner = remove_slice(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_TAGGED: {
            if (get_term_tag(c, type) == TIR_TAGGED_TYPE) {
                TirId inner = remove_tags(c, type);
                return match_types_single(c, results, inner, matcher->inner);
            }
            return 0;
        }
    }
    return 0;
}

int match_types(TirContext c, TirId *results, int32_t count, TirId *types, TypeMatcher *matchers) {
    for (int32_t i = 0; i < count; i++) {
        if (!match_types_single(c, results, types[i], &matchers[i])) {
            return 0;
        }
    }
    return 1;
}

int match_type_parameters(TirContext c, TirId *results, TirId param, TirId arg) {
    if (param.id == arg.id) {
        return 1;
    }

    TirTag tag = get_term_tag(c, param);
    if (tag == TIR_TYPE_PARAMETER) {
        int32_t index = get_type_parameter_index(c, param);
        if (!results[index].id) {
            results[index] = arg;
        } else if (results[index].id != arg.id) {
            return 0;
        }
        return 1;
    }
    if (tag != get_term_tag(c, arg)) {
        return 0;
    }
    switch (tag) {
        case TIR_PRIMITIVE_TYPE: {
            return 1;
        }
        case TIR_ARRAY_TYPE: {
            ArrayType param_array = get_array_type(c, param);
            ArrayType arg_array = get_array_type(c, arg);
            return match_type_parameters(c, results, param_array.index, arg_array.index)
                && match_type_parameters(c, results, param_array.elem, arg_array.elem);
        }
        case TIR_ARRAY_LENGTH_TYPE: {
            return 0;
        }
        case TIR_PTR_TYPE:
        case TIR_MUT_PTR_TYPE:
        case TIR_SLICE_TYPE:
        case TIR_MUT_SLICE_TYPE:
        case TIR_AFFINE_TYPE: {
            TirId param_elem = get_type_elem(c, param);
            TirId arg_elem = get_type_elem(c, arg);
            return match_type_parameters(c, results, param_elem, arg_elem);
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType param_f = get_function_type(c, param);
            FunctionType arg_f = get_function_type(c, arg);
            if (param_f.param_count != arg_f.param_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_f.param_count; i++) {
                if (!match_type_parameters(c, results, get_function_type_param(c, param, i), get_function_type_param(c, arg, i))) {
                    return 0;
                }
            }
            return match_type_parameters(c, results, param_f.ret, arg_f.ret);
        }
        case TIR_TAGGED_TYPE: {
            TaggedType param_t = get_tagged_type(c, param);
            TaggedType arg_t = get_tagged_type(c, arg);
            if (param_t.name != arg_t.name) {
                return 0;
            }
            if (param_t.arg_count != arg_t.arg_count) {
                return 0;
            }
            for (int32_t i = 0; i < param_t.arg_count; i++) {
                if (!match_type_parameters(c, results, get_tagged_type_arg(c, param, i), get_tagged_type_arg(c, arg, i))) {
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
    switch (get_term_tag(info->c, generic)) {
        case TIR_PRIMITIVE_TYPE:
        case TIR_ARRAY_LENGTH_TYPE:
        case TIR_ENUM_TYPE: {
            return generic;
        }
        case TIR_TYPE_PARAMETER: {
            int32_t index = get_type_parameter_index(info->c, generic);
            return info->args[index];
        }
        case TIR_ARRAY_TYPE: {
            ArrayType array = get_array_type(info->c, generic);
            return new_array_type(info->c, &(ArrayType) {
                .index = replace_type_parameters(array.index, info),
                .elem = replace_type_parameters(array.elem, info),
            });
        }
        case TIR_PTR_TYPE: {
            TirId elem = get_type_elem(info->c, generic);
            return new_ptr_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_MUT_PTR_TYPE: {
            TirId elem = get_type_elem(info->c, generic);
            return new_mut_ptr_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_SLICE_TYPE: {
            TirId elem = get_type_elem(info->c, generic);
            return new_slice_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_MUT_SLICE_TYPE: {
            TirId elem = get_type_elem(info->c, generic);
            return new_mut_slice_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_AFFINE_TYPE: {
            TirId elem = get_type_elem(info->c, generic);
            return new_affine_type(info->c, replace_type_parameters(elem, info));
        }
        case TIR_FUNCTION_TYPE: {
            FunctionType f = get_function_type(info->c, generic);
            TirId *params = arena_alloc(&info->scratch, TirId, f.param_count);
            for (int32_t i = 0; i < f.param_count; i++) {
                params[i] = replace_type_parameters(get_function_type_param(info->c, generic, i), info);
            }
            return new_function_type(info->c, &(FunctionType) {
                .param_count = f.param_count,
                .params = params,
                .ret = replace_type_parameters(f.ret, info),
            });
        }
        case TIR_TAGGED_TYPE: {
            TaggedType t = get_tagged_type(info->c, generic);
            TirId *tags = arena_alloc(&info->scratch, TirId, t.arg_count);
            for (int32_t i = 0; i < t.arg_count; i++) {
                tags[i] = replace_type_parameters(get_tagged_type_arg(info->c, generic, i), info);
            }
            TirId inner = replace_type_parameters(t.inner, info);
            return new_tagged_type(info->c, &(TaggedType) {
                .name = t.name,
                .inner = inner,
                .arg_count = t.arg_count,
                .args = tags,
            });
        }
        case TIR_STRUCT_TYPE: {
            StructType t = get_struct_type(info->c, generic);
            TirId *fields = arena_alloc(&info->scratch, TirId, t.field_count);
            for (int32_t i = 0; i < t.field_count; i++) {
                fields[i] = replace_type_parameters(get_struct_type_field(info->c, generic, i), info);
            }
            return new_struct_type(info->c, info->target, &(StructType) {
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

TirId new_int_constant(TirContext c, TirId type, int64_t x) {
    TermList *terms = ctx_terms(c);
    uint32_t low, high;
    store_i64(x, &low, &high);
    vec_push(&terms->extra, low);
    vec_push(&terms->extra, high);
    return new_tir(c, TIR_CONST_INT, (TermData) {
        .a = type.id,
        .b = terms->extra.len - 2,
    });
}

TirId new_float_constant(TirContext c, TirId type, double x) {
    TermList *terms = ctx_terms(c);
    uint32_t low, high;
    store_f64(x, &low, &high);
    vec_push(&terms->extra, low);
    vec_push(&terms->extra, high);
    return new_tir(c, TIR_CONST_FLOAT, (TermData) {
        .a = type.id,
        .b = terms->extra.len - 2,
    });
}

TirId new_null_constant(TirContext c, TirId type) {
    return new_tir(c, TIR_CONST_NULL, (TermData) {
        .a = type.id,
    });
}

TirId new_string_constant(TirContext c, TirId type, int32_t s) {
    return new_tir(c, TIR_STRING, (TermData) {
        .a = type.id,
        .b = s,
    });
}

TirId new_function(TirContext c, TirId type, int32_t name) {
    return new_tir(c, TIR_FUNCTION, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_extern_function(TirContext c, TirId type, int32_t name) {
    return new_tir(c, TIR_EXTERN_FUNCTION, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_extern_var(TirContext c, TirId type, int32_t name) {
    return new_tir(c, TIR_EXTERN_VAR, (TermData) {
        .a = type.id,
        .b = name,
    });
}

TirId new_variable(TirContext c, AstId node, TirId type, int32_t index, TirTag tag) {
    return new_tir(
        c,
        tag,
        (TermData) {
            .node = node,
            .a = type.id,
            .b = index,
        }
    );
}

TirId new_unary_tir(TirContext c, TirTag tag, AstId node, TirId type, TirId a) {
    return new_tir(c, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a.id,
    });
}

TirId new_binary_tir(TirContext c, TirTag tag, AstId node, TirId type, TirId a, TirId b) {
    return new_tir(c, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a.id,
        .c = b.id,
    });
}

TirId new_instr(TirContext c, TirTag tag, AstId node, TirId type, int32_t a, int32_t b) {
    return new_tir(c, tag, (TermData) {
        .node = node,
        .a = type.id,
        .b = a,
        .c = b,
    });
}

TirId new_generic(
    TirContext c,
    TirId inner,
    int32_t type_count,
    TirId *types
) {
    TermList *terms = ctx_terms(c);
    int32_t extra = terms->extra.len;
    vec_push(&terms->extra, type_count);
    for (int32_t i = 0; i < type_count; i++) {
        vec_push(&terms->extra, types[i].id);
    }
    return new_term(c, TIR_GENERIC, inner.id, extra);
}

TirId get_value_type(TirContext c, TirId value) {
    if (!is_tir_value(get_term_tag(c, value))) {
        return null_tir;
    }

    return (TirId) {get_term_data(c, value)->a};
}

ValueCategory get_value_category(TirContext c, TirId value) {
    switch (get_term_tag(c, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL:
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
        default: break;
    }
    return VALUE_INVALID;
}

bool is_value_mutable(TirContext c, TirId value) {
    switch (get_term_tag(c, value)) {
        case TIR_FUNCTION:
        case TIR_EXTERN_FUNCTION:
        case TIR_CONST_INT:
        case TIR_CONST_FLOAT:
        case TIR_CONST_NULL:
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
            TirId operand = {get_term_data(c, value)->b};
            switch (get_term_tag(c, get_value_type(c, operand))) {
                case TIR_PTR_TYPE: return false;
                case TIR_MUT_PTR_TYPE: return true;
                default: break;
            }
            break;
        }
        case TIR_INDEX:
        case TIR_SLICE:
        case TIR_ACCESS: {
            TirId operand = {get_term_data(c, value)->b};
            return is_value_mutable(c, operand);
        }
        default: break;
    }
    return false;
}

char const *get_value_str(TirContext c, TirId value) {
    TermIndex i = get_term_index(c, value);
    return tir_get_str(c, i.deps->terms.terms.datas[i.index].b);
}

int64_t get_value_int(TirContext c, TirId value) {
    TermIndex i = get_term_index(c, value);
    int32_t *p = &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
    return load_i64(p[0], p[1]);
}

double get_value_float(TirContext c, TirId value) {
    TermIndex i = get_term_index(c, value);
    int32_t *p = &i.deps->terms.extra.ptr[i.deps->terms.terms.datas[i.index].b];
    return load_f64(p[0], p[1]);
}

char const *tir_get_str(TirContext c, int32_t s) {
    return s < 0 ? &c.thread->strtab.ptr[~s] : &c.global->strtab.ptr[s];
}

int32_t tir_push_str(TirContext c, String s) {
    Tir *tir = ctx_write(c);
    int32_t index = push_str(&tir->strtab, s);
    return c.thread ? ~index : index;
}

int32_t tir_push_cstr(TirContext c, String s) {
    Tir *tir = ctx_write(c);
    int32_t index = push_str(&tir->strtab, s);
    push_str(&tir->strtab, (String) {1, ""});
    return c.thread ? ~index : index;
}
