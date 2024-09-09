#include "data/tir.h"

#include "arena.h"
#include "enums.h"
#include "fwd.h"
#include "util.h"

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// Type set

static size_t hash_type(TirContext ctx, TypeId type) {
    size_t result = 17;
    TypeTag tag = get_type_tag(ctx, type);
    result = 31 * result + tag;
    switch (tag) {
        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx, type);
            result = 31 * result + hash_type(ctx, array.index);
            result = 31 * result + hash_type(ctx, array.elem);
            break;
        }
        case TYPE_ARRAY_LENGTH: {
            int64_t length = get_array_length_type(ctx, type);
            result = 31 * result + length;
            break;
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: {
            result = 31 * result + hash_type(ctx, remove_any_pointer(ctx, type));
            break;
        }
        case TYPE_FUNCTION: {
            FunctionType f = get_function_type(ctx, type);
            result = 31 * result + f.type_param_count;
            result = 31 * result + f.param_count;
            for (int64_t i = 0; i < f.param_count; i++) {
                result = 31 * result + hash_type(ctx, get_function_type_param(ctx, type, i));
            }
            if (f.ret.id != -1) {
                result = 31 * result + hash_type(ctx, f.ret);
            }
            break;
        }
        case TYPE_TAGGED: {
            TaggedType t = get_tagged_type(ctx, type);
            result = 31 * result + t.newtype.id;
            result = 31 * result + t.arg_count;
            for (int64_t i = 0; i < t.arg_count; i++) {
                result = 31 * result + hash_type(ctx, get_tagged_type_arg(ctx, type, i));
            }
            break;
        }
        case TYPE_PRIMITIVE:
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_NEWTYPE: {
            result = 31 * result + type.id;
            break;
        }
        case TYPE_LINEAR: {
            result = 31 * result + hash_type(ctx, get_linear_elem_type(ctx, type));
            break;
        }
        case TYPE_TYPE_PARAMETER: {
            result = 31 * result + get_type_parameter_index(ctx, type);
            break;
        }
    }
    return result;
}

static size_t find_type_entry(TypeSet const *set, TypeId key, TirContext ctx) {
    size_t index = hash_type(ctx, key) & (set->capacity - 1);
    for (;;) {
        if (!set->ptr[index].id) {
            return SIZE_MAX;
        }
        if (type_eq(ctx, set->ptr[index], key)) {
            return index;
        }
        index = (index + 1) & (set->capacity - 1);
    }
    return SIZE_MAX;
}

static void typeset_init(TypeSet *set, size_t capacity) {
    set->capacity = capacity;
    set->count = 0;
    set->ptr = calloc(capacity, sizeof(*set->ptr));
    if (!set->ptr) {
        abort();
    }
}

static void typeset_free(TypeSet *set) {
    free(set->ptr);
}

/*
    If set contains key, return existing TypeId.
    Otherwise, add and return key.
*/
static void typeset_insert_entry(TypeSet *set, TypeId key, TirContext ctx) {
    assert(set);
    assert(key.id);
    size_t index = hash_type(ctx, key) & (set->capacity - 1);
    while (set->ptr[index].id) {
        index = (index + 1) & (set->capacity - 1);
    }
    set->ptr[index] = key;
}

static void typeset_resize(TypeSet *set, TirContext ctx) {
    TypeSet new_set;
    typeset_init(&new_set, set->capacity * 2);
    new_set.count = set->count;
    for (size_t i = 0; i < set->capacity; i++) {
        if (set->ptr[i].id) {
            typeset_insert_entry(&new_set, set->ptr[i], ctx);
        }
    }
    typeset_free(set);
    *set = new_set;
}

static TypeId typeset_try_insert(TypeSet *set, TypeId key, TirContext ctx) {
    if (set->capacity == 0) {
        typeset_init(set, 64);
    } else if (set->count * 4 / set->capacity >= 3) {
        typeset_resize(set, ctx);
    }

    size_t index = hash_type(ctx, key) & (set->capacity - 1);
    while (set->ptr[index].id) {
        if (type_eq(ctx, set->ptr[index], key)) {
            return set->ptr[index];
        }
        index = (index + 1) & (set->capacity - 1);
    }

    set->ptr[index] = key;
    set->count++;
    return key;
}

static TypeId typeset_contains(TypeSet const *set, TypeId key, TirContext ctx) {
    size_t entry = find_type_entry(set, key, ctx);
    if (entry == SIZE_MAX) {
        return key;
    }
    return set->ptr[entry];
}

static TypeId new_type(TirContext ctx, TypeTag tag, TypeData const *data, int32_t extra_count) {
    TypeId tmp = {ctx.global->types.types.len + TYPE_COUNT};
    TypeList *types = &ctx.global->types;
    if (ctx.thread) {
        types = &ctx.thread->deps.types;
        tmp.id += ctx.thread->deps.types.types.len;
    }
    sum_vec_push(&types->types, *data, tag);

    if (ctx.thread) {
        // Check if type already exists in thread.
        TypeId type = typeset_try_insert(&ctx.thread->deps.types.set, tmp, ctx);
        if (type.id == tmp.id) {
            // Check if type already exists in global.
            type = typeset_contains(&ctx.global->types.set, tmp, ctx);
        }

        if (type.id != tmp.id) {
            ctx.thread->deps.types.types.len--;
            ctx.thread->deps.types.extra.len -= extra_count;
        }

        return type;
    }

    TypeId type = typeset_try_insert(&ctx.global->types.set, tmp, ctx);
    if (type.id != tmp.id) {
        ctx.global->types.types.len--;
        ctx.global->types.extra.len -= extra_count;
    }

    return type;
}

// Types

static TypeList *ctx_types(TirContext ctx) {
    return ctx.thread ? &ctx.thread->deps.types : &ctx.global->types;
}

TypeId new_array_type(TirContext ctx, TypeId index, TypeId element) {
    TypeData data = {
        .index = element.id,
        .extra = index.id,
    };
    return new_type(ctx, TYPE_ARRAY, &data, 0);
}

TypeId new_array_length_type(TirContext ctx, int64_t length) {
    return new_type(ctx, TYPE_ARRAY_LENGTH, (TypeData *) &length, 0);
}

TypeId new_ptr_type(TirContext ctx, TypeTag tag, TypeId elem) {
    TypeData data = {
        .index = elem.id,
        .extra = 0,
    };
    return new_type(ctx, tag, &data, 0);
}

TypeId new_multiptr_type(TirContext ctx, TypeTag tag, TypeId elem) {
    TypeData data = {
        .index = elem.id,
        .extra = new_ptr_type(ctx, tag == TYPE_MULTIPTR_MUT ? TYPE_PTR_MUT : TYPE_PTR, type_byte).id,
    };
    return new_type(ctx, tag, &data, 0);
}

TypeId new_function_type(TirContext ctx, int32_t type_param_count, int32_t param_count, TypeId const *params, TypeId ret) {
    TypeList *types = ctx_types(ctx);
    int32_t index = types->extra.len;
    vec_push(&types->extra, type_param_count);
    vec_push(&types->extra, ret.id);
    for (int32_t i = 0; i < param_count; i++) {
        vec_push(&types->extra, params[i].id);
    }
    TypeData data = {
        .index = index,
        .extra = param_count,
    };
    return new_type(ctx, TYPE_FUNCTION, &data, param_count + 2);
}

typedef struct {
    int32_t scope;
    int32_t name;
    int32_t alignment;
    int32_t size;
    int32_t is_linear;
    int32_t type_param_count;
} StructTypeLayout;

typedef struct {
    int32_t tags;
    int32_t name;
} NewtypeLayout;

static void init_struct_layout(StructTypeLayout *layout, TirContext ctx, int32_t field_count, TypeId const *fields, Target target) {
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

TypeId new_struct_type(TirContext ctx, int32_t scope, int32_t name, int32_t type_param_count, int32_t field_count, TypeId const *fields, Target target) {
    TypeList *types = ctx_types(ctx);
    int32_t index = types->extra.len;
    int32_t *ptr = vec_grow(&types->extra, field_count + sizeof(StructTypeLayout) / sizeof(int32_t));
    StructTypeLayout *layout = (StructTypeLayout *) ptr;
    layout->scope = scope;
    layout->name = name;
    init_struct_layout(layout, ctx, field_count, fields, target);
    layout->is_linear = false;
    layout->type_param_count = type_param_count;
    for (int32_t i = 0; i < field_count; i++) {
        ptr[i + sizeof(StructTypeLayout) / sizeof(int32_t)] = fields[i].id;
        if (!layout->is_linear && type_is_linear(ctx, fields[i])) {
            layout->is_linear = true;
        }
    }
    TypeData data = {
        .index = index,
        .extra = field_count,
    };
    return new_type(ctx, TYPE_STRUCT, &data, field_count + sizeof(StructTypeLayout) / sizeof(int32_t));
}

TypeId new_enum_type(TirContext ctx, int32_t scope, int32_t name, TypeId repr) {
    TypeList *types = ctx_types(ctx);
    int32_t index = types->extra.len;
    vec_push(&types->extra, scope);
    vec_push(&types->extra, name);
    TypeData data = {
        .index = index,
        .extra = repr.id,
    };
    return new_type(ctx, TYPE_ENUM, &data, 0);
}

TypeId new_newtype_type(TirContext ctx, int32_t name, int32_t tags, TypeId type) {
    TypeList *types = ctx_types(ctx);
    int32_t index = types->extra.len;
    NewtypeLayout *layout = (NewtypeLayout *) vec_grow(&types->extra, sizeof(NewtypeLayout) / sizeof(int32_t));
    layout->tags = tags;
    layout->name = name;
    TypeData data = {
        .index = index,
        .extra = type.id,
    };
    return new_type(ctx, TYPE_NEWTYPE, &data, sizeof(NewtypeLayout) / sizeof(int32_t));
}

TypeId new_tagged_type(TirContext ctx, TypeId newtype, TypeId inner, int32_t arg_count, TypeId const *args) {
    TypeList *types = ctx_types(ctx);
    int32_t index = types->extra.len;
    vec_push(&types->extra, newtype.id);
    vec_push(&types->extra, inner.id);
    for (int32_t i = 0; i < arg_count; i++) {
        vec_push(&types->extra, args[i].id);
    }
    TypeData data = {
        .index = index,
        .extra = arg_count,
    };
    return new_type(ctx, TYPE_TAGGED, &data, arg_count + 2);
}

TypeId new_linear_type(TirContext ctx, TypeId elem) {
    if (get_type_tag(ctx, elem) == TYPE_LINEAR) {
        return elem;
    }
    TypeData data = {
        .index = elem.id,
        .extra = 0,
    };
    return new_type(ctx, TYPE_LINEAR, &data, 0);
}

TypeId new_type_parameter(TirContext ctx, int32_t i, int32_t name) {
    TypeData data = {
        .index = i,
        .extra = name,
    };
    return new_type(ctx, TYPE_TYPE_PARAMETER, &data, 0);
}

typedef struct {
    TirDependencies *deps;
    int32_t index;
} TypeIndex;

static TypeIndex get_type_index(TirContext ctx, TypeId type) {
    if (type.id - TYPE_COUNT < ctx.global->types.types.len) {
        return (TypeIndex) {ctx.global, type.id - TYPE_COUNT};
    }
    return (TypeIndex) {&ctx.thread->deps, type.id - TYPE_COUNT - ctx.global->types.types.len};
}

TypeTag get_type_tag(TirContext ctx, TypeId type) {
    if (type.id < TYPE_COUNT) {
        return TYPE_PRIMITIVE;
    }
    TypeIndex i = get_type_index(ctx, type);
    return i.deps->types.types.tags[i.index];
}

static TypeData const *get_type_data(TirContext ctx, TypeId type) {
    if (type.id < TYPE_COUNT) {
        return NULL;
    }
    TypeIndex i = get_type_index(ctx, type);
    return &i.deps->types.types.datas[i.index];
}

static int32_t *get_type_extra(TirContext ctx, TypeId type) {
    if (type.id < TYPE_COUNT) {
        return NULL;
    }
    TypeIndex i = get_type_index(ctx, type);
    return &i.deps->types.extra.ptr[i.deps->types.types.datas[i.index].index];
}

TypeId remove_any_pointer(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return (TypeId) {get_type_data(ctx, type)->index};

        default: return null_type;
    }
}

TypeId remove_pointer(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PTR:
        case TYPE_PTR_MUT: return (TypeId) {get_type_data(ctx, type)->index};

        default: return null_type;
    }
}

TypeId remove_slice(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return (TypeId) {get_type_data(ctx, type)->index};

        default: return null_type;
    }
}

TypeId replace_slice_with_pointer(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_MULTIPTR: return new_ptr_type(ctx, TYPE_PTR, (TypeId) {get_type_data(ctx, type)->index});
        case TYPE_MULTIPTR_MUT: return new_ptr_type(ctx, TYPE_PTR_MUT, (TypeId) {get_type_data(ctx, type)->index});
        default: return null_type;
    }
}

TypeId replace_pointer_with_slice(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PTR: return new_ptr_type(ctx, TYPE_MULTIPTR, (TypeId) {get_type_data(ctx, type)->index});
        case TYPE_PTR_MUT: return new_ptr_type(ctx, TYPE_MULTIPTR_MUT, (TypeId) {get_type_data(ctx, type)->index});
        default: return null_type;
    }
}

TypeId remove_c_pointer_like(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_ARRAY:
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return (TypeId) {get_type_data(ctx, type)->index};

        default: return null_type;
    }
}

TypeId remove_array_like(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_ARRAY:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return (TypeId) {get_type_data(ctx, type)->index};

        default: return null_type;
    }
}

TypeId remove_tags(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_TAGGED) {
        return type;
    }

    return get_tagged_type(ctx, type).inner;
}

bool is_aggregate_type(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PRIMITIVE:
        case TYPE_ARRAY_LENGTH:
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_FUNCTION:
        case TYPE_ENUM: return false;

        case TYPE_ARRAY:
        case TYPE_STRUCT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT:
        case TYPE_TYPE_PARAMETER: return true;

        case TYPE_TAGGED: return is_aggregate_type(ctx, get_tagged_type(ctx, type).inner);
        case TYPE_NEWTYPE: return is_aggregate_type(ctx, get_newtype_type(ctx, type).type);
        case TYPE_LINEAR: return is_aggregate_type(ctx, get_linear_elem_type(ctx, type));
    }
    return false;
}

bool type_is_linear(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_ARRAY: return type_is_linear(ctx, get_array_type(ctx, type).elem);
        case TYPE_TAGGED: return type_is_linear(ctx, get_tagged_type(ctx, type).inner);
        case TYPE_STRUCT: return get_struct_type(ctx, type).is_linear;
        case TYPE_LINEAR: return true;
        default: return false;
    }
}

bool type_is_unknown_size(TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_ARRAY: return type_is_unknown_size(ctx, get_array_type(ctx, type).elem);
        case TYPE_TAGGED: return type_is_unknown_size(ctx, get_tagged_type(ctx, type).inner);
        case TYPE_LINEAR: return type_is_unknown_size(ctx, get_linear_elem_type(ctx, type));
        case TYPE_TYPE_PARAMETER: return true;
        default: return false;
    }
}

static bool array_type_eq(TirContext ctx, TypeId a, TypeId b) {
    ArrayType a_array = get_array_type(ctx, a);
    ArrayType b_array = get_array_type(ctx, b);
    return type_eq(ctx, a_array.index, b_array.index) && type_eq(ctx, a_array.elem, b_array.elem);
}

static bool function_type_eq(TirContext ctx, TypeId a, TypeId b) {
    FunctionType a_f = get_function_type(ctx, a);
    FunctionType b_f = get_function_type(ctx, b);
    if (a_f.type_param_count != b_f.type_param_count || a_f.type_param_count) {
        return false;
    }
    if (a_f.param_count != b_f.param_count) {
        return false;
    }
    for (int64_t i = 0; i < a_f.param_count; i++) {
        if (!type_eq(ctx, get_function_type_param(ctx, a, i), get_function_type_param(ctx, b, i))) {
            return false;
        }
    }
    if (a_f.ret.id != -1 && b_f.ret.id != -1) {
        return type_eq(ctx, a_f.ret, b_f.ret);
    }
    return true;
}

static bool tagged_type_eq(TirContext ctx, TypeId a, TypeId b) {
    TaggedType a_t = get_tagged_type(ctx, a);
    TaggedType b_t = get_tagged_type(ctx, a);
    if (a_t.newtype.id != b_t.newtype.id) {
        return false;
    }
    if (a_t.arg_count != b_t.arg_count) {
        return false;
    }
    for (int32_t i = 0; i < a_t.arg_count; i++) {
        if (!type_eq(ctx, get_tagged_type_arg(ctx, a, i), get_tagged_type_arg(ctx, b, i))) {
            return false;
        }
    }
    return true;
}

bool type_eq(TirContext ctx, TypeId a, TypeId b) {
    if (!a.id || !b.id) {
        return false;
    }
    if (a.id == b.id) {
        return true;
    }
    if (get_type_tag(ctx, a) != get_type_tag(ctx, b)) {
        return false;
    }
    switch (get_type_tag(ctx, a)) {
        case TYPE_PRIMITIVE:
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_NEWTYPE:
        case TYPE_TYPE_PARAMETER: return false;

        case TYPE_ARRAY: return array_type_eq(ctx, a, b);
        case TYPE_ARRAY_LENGTH: return get_array_length_type(ctx, a) == get_array_length_type(ctx, b);

        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return type_eq(ctx, remove_any_pointer(ctx, a), remove_any_pointer(ctx, b));

        case TYPE_FUNCTION: return function_type_eq(ctx, a, b);
        case TYPE_TAGGED: return tagged_type_eq(ctx, a, b);
        case TYPE_LINEAR: return tagged_type_eq(ctx, a, b);
    }
    abort();
}

bool is_equality_type(TirContext ctx, TypeId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (a.id == TYPE_bool) {
        return true;
    }
    if (a.id == TYPE_byte) {
        return true;
    }
    switch (get_type_tag(ctx, a)) {
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_FUNCTION:
        case TYPE_ENUM: return true;

        default: return false;
    }
}

bool is_relative_type(TirContext ctx, TypeId a) {
    if (type_is_arithmetic(a)) {
        return true;
    }
    if (a.id == TYPE_byte) {
        return true;
    }
    switch (get_type_tag(ctx, a)) {
        case TYPE_ENUM: return true;
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

static int64_t sizeof_primitive(TypeId type, Target target) {
    switch ((PrimitiveType) type.id) {
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

        case TYPE_SIZE_TAG:
        case TYPE_ALIGNMENT_TAG:
        case TYPE_isize: return sizeof_pointer(target);
        case TYPE_COUNT: break;
    }
    abort();
}

bool int_fits_in_type(int64_t i, TypeId type, Target target) {
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

TypeId bigger_primitive_type(TypeId a, TypeId b, Target target) {
    return sizeof_primitive(a, target) > sizeof_primitive(b, target) ? a : b;
}

ArrayType get_array_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_ARRAY) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    ArrayType array = {
        .index = {data->extra},
        .elem = {data->index},
    };
    return array;
}

int64_t get_array_length_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_ARRAY_LENGTH) {
        abort();
    }

    return *(int64_t const *) get_type_data(ctx, type);
}

TypeId get_linear_elem_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_LINEAR) {
        abort();
    }

    return (TypeId) {get_type_data(ctx, type)->index};
}

int32_t get_type_parameter_index(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_TYPE_PARAMETER) {
        abort();
    }

    return get_type_data(ctx, type)->index;
}

FunctionType get_function_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_FUNCTION) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    FunctionType function = {
        .type_param_count = extra[0],
        .param_count = data->extra,
        .ret = {extra[1]},
    };
    return function;
}

TypeId get_function_type_param(TirContext ctx, TypeId type, int32_t index) {
    if (get_type_tag(ctx, type) != TYPE_FUNCTION) {
        return null_type;
    }

    TypeData const *data = get_type_data(ctx, type);
    if (index >= data->extra) {
        return null_type;
    }
    return (TypeId) {get_type_extra(ctx, type)[index + 2]};
}

StructType get_struct_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_STRUCT) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    StructTypeLayout *layout = (StructTypeLayout *) extra;
    StructType s = {
        .scope = layout->scope,
        .name = layout->name,
        .alignment = layout->alignment,
        .size = layout->size,
        .type_param_count = layout->type_param_count,
        .field_count = data->extra,
        .is_linear = layout->is_linear,
    };
    return s;
}

TypeId get_struct_type_field(TirContext ctx, TypeId type, int32_t index) {
    if (get_type_tag(ctx, type) != TYPE_STRUCT) {
        return null_type;
    }

    TypeData const *data = get_type_data(ctx, type);
    if (index >= data->extra) {
        return null_type;
    }
    return (TypeId) {get_type_extra(ctx, type)[index + sizeof(StructTypeLayout) / sizeof(int32_t)]};
}

TypeId get_any_struct_type_field(TirContext ctx, TypeId type, int32_t index) {
    if (get_type_tag(ctx, type) == TYPE_MULTIPTR || get_type_tag(ctx, type) == TYPE_MULTIPTR_MUT) {
        switch (index) {
            case 0: return type_isize;
            case 1: return (TypeId) {get_type_data(ctx, type)->extra};
            default: return null_type;
        }
    }

    if (get_type_tag(ctx, type) != TYPE_STRUCT) {
        return null_type;
    }

    TypeData const *data = get_type_data(ctx, type);
    if (index >= data->extra) {
        return null_type;
    }
    return (TypeId) {get_type_extra(ctx, type)[index + sizeof(StructTypeLayout) / sizeof(int32_t)]};
}

EnumType get_enum_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_ENUM) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    EnumType e = {
        .scope = extra[0],
        .name = extra[1],
        .repr = {data->extra},
    };
    return e;
}

NewtypeType get_newtype_type(TirContext ctx, TypeId type) {
    if (type.id == TYPE_SIZE_TAG || type.id == TYPE_ALIGNMENT_TAG) {
        NewtypeType n = {
            .tags = 1,
            .name = 0,
            .type = type_isize,
        };
        return n;
    }

    if (get_type_tag(ctx, type) != TYPE_NEWTYPE) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    NewtypeType n = {
        .tags = extra[0],
        .name = extra[1],
        .type = {data->extra},
    };
    return n;
}

TaggedType get_tagged_type(TirContext ctx, TypeId type) {
    if (get_type_tag(ctx, type) != TYPE_TAGGED) {
        abort();
    }

    TypeData const *data = get_type_data(ctx, type);
    int32_t *extra = get_type_extra(ctx, type);
    TaggedType t = {
        .newtype = {extra[0]},
        .inner = {extra[1]},
        .arg_count = data->extra,
    };
    return t;
}

TypeId get_tagged_type_arg(TirContext ctx, TypeId type, int32_t index) {
    if (get_type_tag(ctx, type) != TYPE_TAGGED) {
        return null_type;
    }

    TypeData const *data = get_type_data(ctx, type);
    if (index >= data->extra) {
        return null_type;
    }
    return (TypeId) {get_type_extra(ctx, type)[index + 2]};
}

int32_t sizeof_pointer(Target target) {
    switch (target) {
        case TARGET_ISIZE_64: return 8;
        case TARGET_ISIZE_32: return 4;
    }
    abort();
}

int32_t alignof_type(TirContext ctx, TypeId type, Target target) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PRIMITIVE: {
            switch ((PrimitiveType) type.id) {
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

                case TYPE_SIZE_TAG:
                case TYPE_ALIGNMENT_TAG:
                case TYPE_isize: return sizeof_pointer(target);

                case TYPE_COUNT: break;
            }
            abort();
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_FUNCTION:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return sizeof_pointer(target);

        case TYPE_ARRAY: return alignof_type(ctx, get_array_type(ctx, type).elem, target);
        case TYPE_ARRAY_LENGTH: return sizeof_pointer(target);
        case TYPE_STRUCT: return get_struct_type(ctx, type).alignment;
        case TYPE_ENUM: return alignof_type(ctx, get_enum_type(ctx, type).repr, target);
        case TYPE_NEWTYPE: return alignof_type(ctx, get_newtype_type(ctx, type).type, target);
        case TYPE_TAGGED: return alignof_type(ctx, get_tagged_type(ctx, type).inner, target);
        case TYPE_LINEAR: return alignof_type(ctx, get_linear_elem_type(ctx, type), target);
        case TYPE_TYPE_PARAMETER: return -1;
    }
    abort();
}

int64_t sizeof_type(TirContext ctx, TypeId type, Target target) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PRIMITIVE: return sizeof_primitive(type, target);

        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_FUNCTION: return sizeof_pointer(target);

        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT: return 2 * sizeof_pointer(target);

        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx, type);
            int64_t length = get_array_length_type(ctx, array.index);
            return length * sizeof_type(ctx, array.elem, target);
        }
        case TYPE_ARRAY_LENGTH: return sizeof_pointer(target);
        case TYPE_STRUCT: return get_struct_type(ctx, type).size;
        case TYPE_ENUM: return sizeof_type(ctx, get_enum_type(ctx, type).repr, target);
        case TYPE_NEWTYPE: return sizeof_type(ctx, get_newtype_type(ctx, type).type, target);
        case TYPE_TAGGED: return sizeof_type(ctx, get_tagged_type(ctx, type).inner, target);
        case TYPE_LINEAR: return sizeof_type(ctx, get_linear_elem_type(ctx, type), target);
        case TYPE_TYPE_PARAMETER: return -1;
    }
    abort();
}

void print_type(FILE *file, TirContext ctx, TypeId type) {
    switch (get_type_tag(ctx, type)) {
        case TYPE_PRIMITIVE: {
            switch ((PrimitiveType) type.id) {
                case TYPE_INVALID: fprintf(file, "{error}"); return;
                case TYPE_VOID: fprintf(file, "void"); return;
                case TYPE_SIZE_TAG: fprintf(file, "`Size"); return;
                case TYPE_ALIGNMENT_TAG: fprintf(file, "`Alignment"); return;

                #define TYPE(type) case TYPE_##type: fprintf(file, #type); return;
                #include "simple-types"

                case TYPE_COUNT: break;
            }
            compiler_error("print_type: unknown primitive type");
        }
        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx, type);
            int64_t length = get_array_length_type(ctx, array.index);
            fprintf(file, "[:%ld]", length);
            print_type(file, ctx, array.elem);
            return;
        }
        case TYPE_ARRAY_LENGTH: {
            int64_t length = get_array_length_type(ctx, type);
            fprintf(file, "`ArrayLength(%ld)", length);
            return;
        }
        case TYPE_PTR: {
            fprintf(file, "*");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TYPE_PTR_MUT: {
            fprintf(file, "*mut ");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TYPE_MULTIPTR: {
            fprintf(file, "@");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TYPE_MULTIPTR_MUT: {
            fprintf(file, "@mut ");
            print_type(file, ctx, remove_any_pointer(ctx, type));
            return;
        }
        case TYPE_FUNCTION: {
            FunctionType f = get_function_type(ctx, type);
            fprintf(file, "function ");
            if (f.type_param_count) {
                fprintf(file, "[");
                for (int32_t j = 0; j < f.type_param_count; j++) {
                    if (j != 0) {
                        fprintf(file, ", ");
                    }
                    fprintf(file, "T#%d", j);
                }
                fprintf(file, "]");
            }
            fprintf(file, "(");
            for (int32_t j = 0; j < f.param_count; j++) {
                if (j != 0) {
                    fprintf(file, ", ");
                }
                TypeId param_type = get_function_type_param(ctx, type, j);
                print_type(file, ctx, param_type);
            }
            fprintf(file, ")");
            if (f.ret.id != -1) {
                fprintf(file, " -> ");
                print_type(file, ctx, f.ret);
            }
            return;
        }
        case TYPE_STRUCT: {
            fprintf(file, "%s", ctx.global->strtab.ptr + get_struct_type(ctx, type).name);
            return;
        }
        case TYPE_ENUM: {
            fprintf(file, "%s", ctx.global->strtab.ptr + get_enum_type(ctx, type).name);
            return;
        }
        case TYPE_NEWTYPE: {
            fprintf(file, "%s", ctx.global->strtab.ptr + get_newtype_type(ctx, type).name);
            return;
        }
        case TYPE_TAGGED: {
            TaggedType t = get_tagged_type(ctx, type);
            print_type(file, ctx, t.newtype);
            fprintf(file, "[");
            for (int32_t j = 0; j < t.arg_count; j++) {
                if (j != 0) {
                    fprintf(file, ", ");
                }
                TypeId arg = get_tagged_type_arg(ctx, type, j);
                print_type(file, ctx, arg);
            }
            fprintf(file, "]");
            return;
        }
        case TYPE_LINEAR: {
            fprintf(file, "`Affine[");
            print_type(file, ctx, get_linear_elem_type(ctx, type));
            fprintf(file, "]");
            return;
        }
        case TYPE_TYPE_PARAMETER: {
            TypeData const *data = get_type_data(ctx, type);
            fprintf(file, "T#%d", data->index);
            return;
        }
    }
    compiler_error("print_type: unknown type tag");
}

void debug_type(TirContext ctx, TypeId type) {
    print_type(stderr, ctx, type);
    printf("\n");
}

// Type matching

static int match_types_single(TirContext ctx, TypeId *results, TypeId type, TypeMatcher *matcher) {
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
            if (get_type_tag(ctx, type) == TYPE_ARRAY) {
                ArrayType array_type = get_array_type(ctx, type);
                return match_types_single(ctx, results, array_type.index, &matcher->inner[0])
                    && match_types_single(ctx, results, array_type.elem, &matcher->inner[1]);
            }
            return 0;
        }
        case TYPE_MATCH_ANY_POINTER: {
            TypeId inner = remove_pointer(ctx, type);
            return match_types_single(ctx, results, inner, matcher->inner);
        }
        case TYPE_MATCH_ANY_SLICE: {
            TypeId inner = remove_slice(ctx, type);
            return match_types_single(ctx, results, inner, matcher->inner);
        }
        case TYPE_MATCH_POINTER: {
            if (get_type_tag(ctx, type) == TYPE_PTR) {
                TypeId inner = remove_pointer(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_SLICE: {
            if (get_type_tag(ctx, type) == TYPE_MULTIPTR) {
                TypeId inner = remove_slice(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_POINTER: {
            if (get_type_tag(ctx, type) == TYPE_PTR_MUT) {
                TypeId inner = remove_pointer(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_MUT_SLICE: {
            if (get_type_tag(ctx, type) == TYPE_MULTIPTR_MUT) {
                TypeId inner = remove_slice(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
        case TYPE_MATCH_TAGGED: {
            if (get_type_tag(ctx, type) == TYPE_TAGGED) {
                TypeId inner = remove_tags(ctx, type);
                return match_types_single(ctx, results, inner, matcher->inner);
            }
            return 0;
        }
    }
    return 0;
}

int match_types(TirContext ctx, TypeId *results, int32_t count, TypeId *types, TypeMatcher *matchers) {
    for (int32_t i = 0; i < count; i++) {
        if (!match_types_single(ctx, results, types[i], &matchers[i])) {
            return 0;
        }
    }
    return 1;
}

int match_type_parameters(TirContext ctx, TypeId *results, TypeId param, TypeId arg) {
    if (param.id == arg.id) {
        return 1;
    }

    TypeTag tag = get_type_tag(ctx, param);
    if (tag == TYPE_TYPE_PARAMETER) {
        int32_t index = get_type_data(ctx, param)->index;
        if (!results[index].id) {
            results[index] = arg;
        } else if (results[index].id != arg.id) {
            return 0;
        }
        return 1;
    }
    if (tag != get_type_tag(ctx, arg)) {
        return 0;
    }
    switch (tag) {
        case TYPE_PRIMITIVE: {
            return 1;
        }
        case TYPE_ARRAY: {
            ArrayType param_array = get_array_type(ctx, param);
            ArrayType arg_array = get_array_type(ctx, arg);
            return match_type_parameters(ctx, results, param_array.index, arg_array.index)
                && match_type_parameters(ctx, results, param_array.elem, arg_array.elem);
        }
        case TYPE_ARRAY_LENGTH: {
            return 0;
        }
        case TYPE_PTR:
        case TYPE_PTR_MUT:
        case TYPE_MULTIPTR:
        case TYPE_MULTIPTR_MUT:
        case TYPE_LINEAR: {
            TypeId param_elem = {get_type_data(ctx, param)->index};
            TypeId arg_elem = {get_type_data(ctx, arg)->index};
            return match_type_parameters(ctx, results, param_elem, arg_elem);
        }
        case TYPE_FUNCTION: {
            FunctionType param_f = get_function_type(ctx, param);
            FunctionType arg_f = get_function_type(ctx, arg);
            if (param_f.type_param_count != arg_f.type_param_count) {
                return 0;
            }
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
        case TYPE_TAGGED: {
            TaggedType param_t = get_tagged_type(ctx, param);
            TaggedType arg_t = get_tagged_type(ctx, arg);
            if (param_t.arg_count != arg_t.arg_count) {
                return 0;
            }
            if (!match_type_parameters(ctx, results, param_t.inner, arg_t.inner)) {
                return 0;
            }
            for (int32_t i = 0; i < param_t.arg_count; i++) {
                if (!match_type_parameters(ctx, results, get_tagged_type_arg(ctx, param, i), get_tagged_type_arg(ctx, arg, i))) {
                    return 0;
                }
            }
            return 1;
        }
        case TYPE_NEWTYPE:
        case TYPE_STRUCT:
        case TYPE_ENUM:
        case TYPE_TYPE_PARAMETER: {
            break;
        }
    }
    return 0;
}

TypeId replace_type_parameters(TirContext ctx, TypeId *args, TypeId generic, Arena scratch) {
    switch (get_type_tag(ctx, generic)) {
        case TYPE_PRIMITIVE: {
            return generic;
        }
        case TYPE_TYPE_PARAMETER: {
            int32_t index = get_type_data(ctx, generic)->index;
            return args[index];
        }
        case TYPE_ARRAY: {
            ArrayType array = get_array_type(ctx, generic);
            return new_array_type(
                ctx,
                replace_type_parameters(ctx, args, array.index, scratch),
                replace_type_parameters(ctx, args, array.elem, scratch)
            );
        }
        case TYPE_ARRAY_LENGTH: {
            return generic;
        }
        case TYPE_PTR: {
            TypeId elem = {get_type_data(ctx, generic)->index};
            return new_ptr_type(ctx, TYPE_PTR, replace_type_parameters(ctx, args, elem, scratch));
        }
        case TYPE_PTR_MUT: {
            TypeId elem = {get_type_data(ctx, generic)->index};
            return new_ptr_type(ctx, TYPE_PTR_MUT, replace_type_parameters(ctx, args, elem, scratch));
        }
        case TYPE_MULTIPTR: {
            TypeId elem = {get_type_data(ctx, generic)->index};
            return new_multiptr_type(ctx, TYPE_MULTIPTR, replace_type_parameters(ctx, args, elem, scratch));
        }
        case TYPE_MULTIPTR_MUT: {
            TypeId elem = {get_type_data(ctx, generic)->index};
            return new_multiptr_type(ctx, TYPE_MULTIPTR_MUT, replace_type_parameters(ctx, args, elem, scratch));
        }
        case TYPE_LINEAR: {
            TypeId elem = {get_type_data(ctx, generic)->index};
            return new_linear_type(ctx, replace_type_parameters(ctx, args, elem, scratch));
        }
        case TYPE_FUNCTION: {
            FunctionType f = get_function_type(ctx, generic);
            if (f.type_param_count) {
                return generic;
            }
            TypeId *params = arena_alloc(&scratch, TypeId, f.param_count);
            for (int32_t i = 0; i < f.param_count; i++) {
                params[i] = replace_type_parameters(ctx, args, get_function_type_param(ctx, generic, i), scratch);
            }
            return new_function_type(ctx, 0, f.param_count, params, replace_type_parameters(ctx, args, f.ret, scratch));
        }
        case TYPE_TAGGED: {
            TaggedType t = get_tagged_type(ctx, generic);
            TypeId *tags = arena_alloc(&scratch, TypeId, t.arg_count);
            for (int32_t i = 0; i < t.arg_count; i++) {
                tags[i] = replace_type_parameters(ctx, args, get_tagged_type_arg(ctx, generic, i), scratch);
            }
            return new_tagged_type(ctx, t.newtype, t.inner, t.arg_count, tags);
        }
        case TYPE_NEWTYPE:
        case TYPE_STRUCT:
        case TYPE_ENUM: {
            return generic;
        }
    }
    return null_type;
}

// Values

static ValueId new_value(TirContext ctx, ValueTag tag, ValueData const *data) {
    if (!ctx.thread) {
        ValueId value = {ctx.global->values.values.len};
        sum_vec_push(&ctx.global->values.values, *data, tag);
        return value;
    }

    ValueId value = {ctx.global->values.values.len + ctx.thread->deps.values.values.len};
    sum_vec_push(&ctx.thread->deps.values.values, *data, tag);
    return value;
}

static ValueList *ctx_values(TirContext ctx) {
    return ctx.thread ? &ctx.thread->deps.values : &ctx.global->values;
}

void init_tir_deps(TirDependencies *deps) {
    ValueData data = {
        .type = null_type,
        .index = 0,
    };
    sum_vec_push(&deps->values.values, data, VAL_ERROR);
}

ValueId new_int_constant(TirContext ctx, TypeId type, int64_t x) {
    ValueList *values = ctx_values(ctx);
    vec_push(&values->extra, x);
    ValueData value = {
        .type = type,
        .index = values->extra.len - 1,
    };
    return new_value(ctx, VAL_CONST_INT, &value);
}

ValueId new_float_constant(TirContext ctx, TypeId type, double x) {
    ValueList *values = ctx_values(ctx);
    uint64_t bits;
    memcpy(&bits, &x, sizeof(x));
    vec_push(&values->extra, bits);
    ValueData value = {
        .type = type,
        .index = values->extra.len - 1,
    };
    return new_value(ctx, VAL_CONST_FLOAT, &value);
}

ValueId new_null_constant(TirContext ctx, TypeId type) {
    ValueData value = {
        .type = type,
        .index = 0,
    };
    return new_value(ctx, VAL_CONST_NULL, &value);
}

ValueId new_string_constant(TirContext ctx, TypeId type, int32_t s) {
    return new_value(ctx, VAL_STRING, &(ValueData) {.type = type, .index = s});
}

ValueId new_function(TirContext ctx, TypeId type, int32_t name) {
    return new_value(ctx, VAL_FUNCTION, &(ValueData) {.type = type, .index = name});
}

ValueId new_extern_function(TirContext ctx, TypeId type, int32_t name) {
    return new_value(ctx, VAL_EXTERN_FUNCTION, &(ValueData) {.type = type, .index = name});
}

ValueId new_extern_var(TirContext ctx, TypeId type, int32_t name) {
    return new_value(ctx, VAL_EXTERN_VAR, &(ValueData) {.type = type, .index = name});
}

ValueId new_variable(TirContext ctx, TypeId type, bool mutable) {
    int32_t local = ctx.thread->local_count++;
    return new_value(ctx, mutable ? VAL_MUTABLE_VARIABLE : VAL_VARIABLE, &(ValueData) {.type = type, .index = local});
}

ValueId new_temporary(TirContext ctx, TypeId type, TirId tir_id) {
    return new_value(ctx, VAL_TEMPORARY, &(ValueData) {.type = type, .index = tir_id.id});
}

typedef struct {
    TirDependencies *deps;
    int32_t index;
} ValueIndex;

static ValueIndex get_value_index(TirContext ctx, ValueId value) {
    if (value.id < ctx.global->values.values.len) {
        return (ValueIndex) {ctx.global, value.id};
    }
    return (ValueIndex) {&ctx.thread->deps, value.id - ctx.global->values.values.len};
}

ValueTag get_value_tag(TirContext ctx, ValueId value) {
    ValueIndex i = get_value_index(ctx, value);
    return i.deps->values.values.tags[i.index];
}

ValueData const *get_value_data(TirContext ctx, ValueId value) {
    ValueIndex i = get_value_index(ctx, value);
    return &i.deps->values.values.datas[i.index];
}

TypeId get_value_type(TirContext ctx, ValueId value) {
    return get_value_data(ctx, value)->type;
}

ValueCategory get_value_category(TirContext ctx, ValueId value) {
    switch (get_value_tag(ctx, value)) {
        case VAL_ERROR: return VALUE_INVALID;

        case VAL_FUNCTION:
        case VAL_EXTERN_FUNCTION:
        case VAL_CONST_INT:
        case VAL_CONST_FLOAT:
        case VAL_CONST_NULL: return VALUE_TEMPORARY;

        case VAL_EXTERN_VAR:
        case VAL_STRING:
        case VAL_VARIABLE: return VALUE_PLACE;

        case VAL_MUTABLE_VARIABLE: return VALUE_MUTABLE_PLACE;

        case VAL_TEMPORARY: {
            TirId tir_id = {get_value_data(ctx, value)->index};
            switch (get_tir_tag(&ctx.thread->insts, tir_id)) {
                case TIR_FUNCTION:
                case TIR_LET:
                case TIR_MUT:
                case TIR_IF:
                case TIR_LOOP:
                case TIR_BREAK:
                case TIR_CONTINUE:
                case TIR_RETURN:
                case TIR_VALUE: abort();

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
                case TIR_SLICE:
                case TIR_NEW_STRUCT:
                case TIR_NEW_ARRAY:
                case TIR_SWITCH: return VALUE_TEMPORARY;

                case TIR_DEREF: {
                    ValueId operand = {get_tir_data(&ctx.thread->insts, tir_id).left};
                    if (get_type_tag(ctx, get_value_type(ctx, operand)) == TYPE_PTR_MUT) {
                        return VALUE_MUTABLE_PLACE;
                    } else {
                        return VALUE_PLACE;
                    }
                }
                case TIR_INDEX: {
                    ValueId operand = {get_tir_data(&ctx.thread->insts, tir_id).left};
                    switch (get_type_tag(ctx, get_value_type(ctx, operand))) {
                        case TYPE_PTR: return VALUE_PLACE;
                        case TYPE_PTR_MUT: return VALUE_MUTABLE_PLACE;
                        default: return get_value_category(ctx, operand);
                    }
                }
                case TIR_ACCESS: {
                    ValueId operand = {get_tir_data(&ctx.thread->insts, tir_id).left};
                    return get_value_category(ctx, operand);
                }
            }
        }
    }
    abort();
}

char const *get_value_str(TirContext ctx, ValueId value) {
    ValueIndex i = get_value_index(ctx, value);
    return &i.deps->strtab.ptr[i.deps->values.values.datas[i.index].index];
}

static uint64_t get_value_constant(TirContext ctx, ValueId value) {
    ValueIndex i = get_value_index(ctx, value);
    return i.deps->values.extra.ptr[i.deps->values.values.datas[i.index].index];
}

int64_t get_value_int(TirContext ctx, ValueId value) {
    uint64_t bits = get_value_constant(ctx, value);
    int64_t result;
    memcpy(&result, &bits, sizeof(bits));
    return result;
}

double get_value_float(TirContext ctx, ValueId value) {
    uint64_t bits = get_value_constant(ctx, value);
    double result;
    memcpy(&result, &bits, sizeof(bits));
    return result;
}

// Instructions

TirTag get_tir_tag(TirInstList *insts, TirId inst) {
    return (TirTag) insts->insts.tags[inst.id];
}

TirInstData get_tir_data(TirInstList *insts, TirId inst) {
    return insts->insts.datas[inst.id];
}

int32_t get_tir_extra(TirInstList *insts, int32_t index) {
    return insts->extra.ptr[index];
}
