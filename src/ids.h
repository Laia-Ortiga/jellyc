#pragma once

#define IdType struct { int32_t private_field_id; }

#define Table(I, V) \
    union { V *private_field_ptr; I *private_field_key; }

#define nth(list, index)                    \
    (_Generic((list).private_field_key,     \
        typeof_unqual(index)*: (list)       \
    ).private_field_ptr[(index).private_field_id])

#define id_as_index(id) \
    ((id).private_field_id)

#define id_is_valid(id) \
    ((id).private_field_id >= 0)
