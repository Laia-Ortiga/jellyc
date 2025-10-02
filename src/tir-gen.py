
# This file generates only the creation and access of tree nodes.
# Everything else can be "safely" done in C through the generated functions.

class Type:
    def __init__(self, c_type, count, c_print = None):
        self.c_type = c_type
        self.count = count
        self.c_print = c_print

node = Type("AstId {}", 1)
ty = Type("TirId {}", 1)
val = Type("TirId {}", 1)
strtab = Type("int32_t {}", 1, c_print="%d")
scope = Type("int32_t {}", 1, c_print="%d")
i32 = Type("int32_t {}", 1, c_print="%d")
i64 = Type("int64_t {}", 2, c_print="%ld")
f64 = Type("double {}", 2, c_print="%f")
boolean = Type("int32_t {}", 1, c_print="%d")

def list_of(T):
    return Type(
        "struct {{ int32_t len; " + T.c_type.format("*ptr") + "; }} {}",
        None
    )

others = [
    {
        "name": "error",
        "fields": [],
    },
    {
        "name": "reserved",
        "fields": [],
    },
    {
        "name": "generic",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "inner", "ty": val },
            { "name": "params", "ty": list_of(ty) },
        ],
    },
    {
        "name": "block",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "stmts", "ty": list_of(ty) },
        ],
    },
]
types = [
    {
        "name": "array_type",
        "fields": [
            { "name": "elem", "ty": ty },
            { "name": "index", "ty": ty },
        ],
    },
    {
        "name": "array_length_type",
        "fields": [
            { "name": "length", "ty": i64 },
        ],
    },
    {
        "name": "ptr_type",
        "names": [
            "ptr_type",
            "mut_ptr_type",
        ],
        "fields": [
            { "name": "elem", "ty": ty },
        ],
    },
    {
        "name": "slice_type",
        "names": [
            "slice_type",
            "mut_slice_type",
        ],
        "fields": [
            { "name": "elem", "ty": ty },
            { "name": "cached_ptr", "ty": ty },
        ],
    },
    {
        "name": "function_type",
        "fields": [
            { "name": "ret", "ty": ty },
            { "name": "params", "ty": list_of(ty) },
        ],
    },
    {
        "name": "tagged_type",
        "fields": [
            { "name": "name", "ty": strtab },
            { "name": "inner", "ty": ty },
            { "name": "args", "ty": list_of(ty) },
        ],
    },
    {
        "name": "struct_type",
        "fields": [
            { "name": "scope", "ty": scope },
            { "name": "name", "ty": strtab },
            { "name": "fields", "ty": list_of(ty) },

            { "name": "alignment", "ty": i32 },
            { "name": "size", "ty": i64 },
            { "name": "is_affine", "ty": boolean },
        ],
    },
    {
        "name": "enum_type",
        "fields": [
            { "name": "scope", "ty": scope },
            { "name": "name", "ty": strtab },
            { "name": "repr", "ty": ty },
        ],
    },
    {
        "name": "affine_type",
        "fields": [
            { "name": "elem", "ty": ty },
        ],
    },
    {
        "name": "type_parameter",
        "fields": [
            { "name": "index", "ty": i32 },
            { "name": "name", "ty": strtab },
        ],
    },
]
values = [
    {
        "name": "function",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "name", "ty": strtab },
        ],
    },
    {
        "name": "extern_function",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "name", "ty": strtab },
        ],
    },
    {
        "name": "extern_var",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "name", "ty": strtab },
        ],
    },
    {
        "name": "int",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "value", "ty": i64 },
        ],
    },
    {
        "name": "float",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "value", "ty": f64 },
        ],
    },
    {
        "name": "null",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
        ],
    },
    {
        "name": "string",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "value", "ty": strtab },
        ],
    },
    {
        "name": "variable",
        "names": [
            "parameter",
            "variable",
            "mutable_variable",
        ],
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "index", "ty": i32 },
        ],
    },
    {
        "name": "let",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "var", "ty": val },
            { "name": "init", "ty": val },
        ],
    },
    {
        "name": "unary",
        "names": [
            "plus",
            "minus",
            "not",
            "deref",
            "address",
            "address_of_temporary",
        ],
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
        ],
    },
    {
        "name": "binary",
        "names": [
            "add",
            "sub",
            "mul",
            "div",
            "mod",
            "and",
            "or",
            "xor",
            "shl",
            "shr",
            "eq",
            "ne",
            "lt",
            "gt",
            "le",
            "ge",
            "assign",
            "assign_add",
            "assign_sub",
            "assign_mul",
            "assign_div",
            "assign_mod",
            "assign_and",
            "assign_or",
            "assign_xor",
        ],
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
            { "name": "b", "ty": val },
        ],
    },
    {
        "name": "cast",
        "names": [
            "itof",
            "itrunc",
            "sext",
            "zext",
            "ftoi",
            "ftrunc",
            "fext",
            "ptr_cast",
            "nop",
            "array_to_slice",
        ],
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
        ],
    },
    {
        "name": "call",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "f", "ty": val },
            { "name": "args", "ty": list_of(val) },
        ],
    },
    {
        "name": "index",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
            { "name": "index", "ty": val },
        ],
    },
    {
        "name": "slice",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
            { "name": "low", "ty": val },
            { "name": "high", "ty": val },
        ],
    },
    {
        "name": "access",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "s", "ty": val },
            { "name": "field", "ty": i32 },
        ],
    },
    {
        "name": "new_struct",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "fields", "ty": list_of(val) },
        ],
    },
    {
        "name": "new_array",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "args", "ty": list_of(val) },
        ],
    },
    {
        "name": "if",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "condition", "ty": val },
            { "name": "true_block", "ty": list_of(val) },
            { "name": "false_block", "ty": list_of(val) },
        ],
    },
    {
        "name": "switch",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "condition", "ty": val },
            { "name": "branches", "ty": list_of(val) },
        ],
    },
    {
        "name": "loop",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "init", "ty": val },
            { "name": "condition", "ty": val },
            { "name": "next", "ty": val },
            { "name": "block", "ty": list_of(val) },
        ],
    },
    {
        "name": "break",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
        ],
    },
    {
        "name": "continue",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
        ],
    },
    {
        "name": "return",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "value", "ty": val },
        ],
    },
]

def join_sets(*sets):
    barriers = []
    terms = []
    i = 0
    for x in sets:
        start = i
        terms += x["set"]
        for v in x["set"]:
            i += len(v.get("names") or [0])
        barriers.append((x["name"], start, i))
    return barriers, terms

barriers, terms = join_sets(
    {"name": None, "set": others},
    {"name": "TYPE", "set": types},
    {"name": "VALUE", "set": values},
)

output = ''

def add_line(x):
    global output
    output += x
    output += '\n'

def to_pascal(x):
    return ''.join(word.capitalize() for word in x.split('_'))

def per_variant(module, v, f):
    if "names" in v:
        for x in v["names"]:
            f(module, { "name": x, "fields": v["fields"] })
    else:
        f(module, v)

def gen_tag(module, v):
    name = v["name"]
    add_line("    {}_{},".format(module.upper(), name.upper()))

def gen_header(module, variants):
    add_line("typedef enum {")
    for v in variants:
        per_variant(module, v, gen_tag)
    add_line("}} {}Tag;".format(to_pascal(module)))
    add_line("")

    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        add_line("typedef struct {")
        for f in v["fields"]:
            add_line("    " + f["ty"].c_type.format(f["name"]) + ";")
        add_line("}} {};".format(to_pascal(module) + to_pascal(name)))
        add_line("")

    # Generate creation functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" in v:
            add_line(to_pascal(module)
                + "Id "
                + module
                + "_push_"
                + v["name"]
                + "(TirContext c, TirTag tag, " + to_pascal(module) + to_pascal(v["name"]) + " a);"
            )
        else:
            add_line(to_pascal(module)
                + "Id "
                + module
                + "_push_"
                + v["name"]
                + "(TirContext c, " + to_pascal(module) + to_pascal(v["name"]) + " a);"
            )

    add_line("")

    # Generate access functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        add_line(to_pascal(module) + to_pascal(name) + " " + module + "_get_" + name + "(TirContext c, TirId a);")

    add_line("#define tir_push(c, ...) \\")
    add_line("    (_Generic((__VA_ARGS__), \\")
    lines = []
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" not in v:
            name = v["name"]
            lines.append("        " + to_pascal(module) + to_pascal(name) + ": tir_push_" + name)
    for i, line in enumerate(lines):
        if i == len(lines) - 1:
            add_line(line + " \\")
        else:
            add_line(line + ", \\")
    add_line("    )(c, __VA_ARGS__))\n")

    add_line("#define tir_push_tag(c, tag, ...) \\")
    add_line("    (_Generic((__VA_ARGS__), \\")
    lines = []
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" in v:
            name = v["name"]
            lines.append("        " + to_pascal(module) + to_pascal(name) + ": tir_push_" + name)
    for i, line in enumerate(lines):
        if i == len(lines) - 1:
            add_line(line + " \\")
        else:
            add_line(line + ", \\")
    add_line("    )(c, tag, __VA_ARGS__))")

def gen_source(module, variants, data_size):
    # Generate creation functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        type_name = to_pascal(module) + to_pascal(v["name"])
        if "names" in v:
            add_line(to_pascal(module)
                + "Id "
                + module
                + "_push_"
                + name
                + "(TirContext c, TirTag tag, " + type_name + " a) {"
            )
            add_line("    switch (tag) {")
            for subname in (v.get("names") or [name]):
                add_line("        case TIR_{}:".format(subname.upper()))
            add_line("            break;")
            add_line("        default:")
            add_line("            abort();")
            add_line("    }")
        else:
            add_line(to_pascal(module)
                + "Id "
                + module
                + "_push_"
                + name
                + "(TirContext c, " + type_name + " a) {"
            )
        add_line("    TermData data;")

        needs_extra = False
        min_count = 0

        for field in v["fields"]:
            if field["ty"].count is None:
                needs_extra = True
                min_count += 1
            else:
                min_count += field["ty"].count

        if min_count > data_size:
            needs_extra = True

        max_data_size = data_size
        index = 0
        if needs_extra:
            max_data_size -= 1

        remaining_fields = []
        for field in v["fields"]:
            if field["ty"].count is None:
                remaining_fields.append(field)
                continue
            if index + field["ty"].count > max_data_size:
                remaining_fields.append(field)
                continue
            add_line("    memcpy(&data.{}, &a.{}, sizeof(a.{}));".format(chr(index + ord('a')), field["name"], field["name"]))
            index += field["ty"].count

        if needs_extra:
            n = 0
            for field in remaining_fields:
                if field["ty"].count is not None:
                    n += field["ty"].count
                else:
                    n += 1
            add_line("    int32_t n = {};".format(n))
            for field in remaining_fields:
                if field["ty"].count is None:
                    add_line("    n += a.{}.len * (sizeof(a.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))

            add_line("    data.{} = tir_writer(c)->terms.extra.len;".format(chr(max_data_size + ord('a'))))
            add_line("    int32_t *extra = vec_grow(&tir_writer(c)->terms.extra, n);")

            for field in remaining_fields:
                if field["ty"].count is None:
                    add_line("    *extra++ = a.{}.len;".format(field["name"]))
                    add_line("    memcpy(extra, a.{}.ptr, a.{}.len * sizeof(a.{}.ptr[0]));".format(field["name"], field["name"], field["name"]))
                    add_line("    extra += a.{}.len * (sizeof(a.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))
                else:
                    add_line("    memcpy(extra, &a.{}, sizeof(a.{}));".format(field["name"], field["name"]))
                    add_line("    extra += sizeof(a.{}) / sizeof(int32_t);".format(field["name"]))

        if "names" in v:
            add_line("    return new_tir(c, tag, data);")
        else:
            add_line("    return new_tir(c, TIR_{}, data);".format(name.upper()))
        add_line("}")
        add_line("")

    # Generate access functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        add_line(to_pascal(module) + to_pascal(name) + " " + module + "_get_" + name + "(TirContext c, TirId a) {")
        add_line("    switch (get_term_tag(c, a)) {")
        for subname in (v.get("names") or [name]):
            add_line("        case TIR_{}:".format(subname.upper()))
        add_line("            break;")
        add_line("        default:")
        add_line("            abort();")
        add_line("    }")

        add_line("    {} result;".format(to_pascal(module) + to_pascal(name)))

        needs_extra = False
        min_count = 0

        for field in v["fields"]:
            if field["ty"].count is None:
                needs_extra = True
                min_count += 1
            else:
                min_count += field["ty"].count

        if min_count > data_size:
            needs_extra = True

        max_data_size = data_size
        index = 0
        if needs_extra:
            max_data_size -= 1

        remaining_fields = []
        for field in v["fields"]:
            if field["ty"].count is None:
                remaining_fields.append(field)
                continue
            if index + field["ty"].count > max_data_size:
                remaining_fields.append(field)
                continue
            add_line("    memcpy(&result.{}, &get_term_data(c, a)->{}, sizeof(result.{}));".format(field["name"], chr(index + ord('a')), field["name"]))
            index += field["ty"].count

        if needs_extra:
            add_line("    int32_t *extra = tir_get_storage(c, a)->terms.extra.ptr + get_term_data(c, a)->{};".format(chr(max_data_size + ord('a'))))

            for field in remaining_fields:
                if field["ty"].count is None:
                    add_line("    result.{}.len = *extra++;".format(field["name"]))
                    add_line("    result.{}.ptr = (void *) extra;".format(field["name"]))
                    add_line("    extra += result.{}.len * (sizeof(result.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))
                else:
                    add_line("    memcpy(&result.{}, extra, sizeof(result.{}));".format(field["name"], field["name"]))
                    add_line("    extra += sizeof(result.{}) / sizeof(int32_t);".format(field["name"]))

        add_line("    return result;")
        add_line("}")
        add_line("")

def gen_print(module, variants):
    add_line("static void print_tir_node(TirPrinter *p, TirId a) {")
    add_line("    switch (get_term_tag(p->tir, a)) {")
    for v in variants:
        name = v["name"]
        type_name = to_pascal(module) + to_pascal(v["name"])
        for subname in (v.get("names") or [name]):
            add_line("        case " + module.upper() + "_" + subname.upper() + ": {")

            if len(v["fields"]) > 0:
                add_line("            " + type_name + " t = tir_get_" + name + "(p->tir, a);")
                add_line("            print_indent(p->depth++);")
                add_line("            printf(\"" + to_pascal(subname) + "(\\n\");")

                for field in v["fields"]:
                    if field["ty"] == ty:
                        add_line("            print_indent(p->depth);")
                        add_line("            printf(\"" + field["name"] + ": \");")
                        add_line("            print_type(stdout, p->tir, t." + field["name"] + ");")
                        add_line("            printf(\",\\n\");")
                    elif field["ty"] == val:
                        add_line("            print_indent(p->depth);")
                        add_line("            printf(\"" + field["name"] + ": \");")
                        add_line("            print_tir_node(p, t." + field["name"] + ");")
                        add_line("            printf(\",\\n\");")
                    elif field["ty"] == strtab:
                        add_line("            print_indent(p->depth);")
                        add_line("            printf(\"" + field["name"] + ": %s,\\n\", tir_get_str(p->tir, t." + field["name"] + "));")
                    elif field["ty"].c_print is not None:
                        add_line("            print_indent(p->depth);")
                        add_line("            printf(\"" + field["name"] + ": " + field["ty"].c_print + ",\\n\", t." + field["name"] + ");")
                    elif field["ty"].count is None:
                        add_line("            print_indent(p->depth);")
                        add_line("            printf(\"" + field["name"] + ": [\");")
                        add_line("            p->depth++;")
                        add_line("            for (int32_t i = 0; i < t." + field["name"] + ".len; i++) {")
                        add_line("                print_tir_node(p, t." + field["name"] + ".ptr[i]);")
                        add_line("            }")
                        add_line("            p->depth--;")
                        add_line("            printf(\"],\\n\");")

                add_line("            print_indent(--p->depth);")
                add_line("            printf(\")\\n\");")
            else:
                add_line("            print_indent(p->depth);")
                add_line("            printf(\"" + to_pascal(subname) + "\\n\");")
            add_line("            break;")
            add_line("        }")
    add_line("    }")
    add_line("}")

output = ''
gen_header("tir", terms)
add_line('')

for b in barriers:
    if b[0] is None:
        continue
    add_line('#define TIR_{}_START {}'.format(b[0], b[1]))
    add_line('#define TIR_{}_END {}'.format(b[0], b[2]))

f = open("src/tir-types.h", "w")
f.write(output)
f.close()

output = '''#include "tir.h"

'''

gen_source("tir", terms, 4)

f = open("src/tir-types.c", "w")
f.write(output)
f.close()

output = ''''''
gen_print("tir", terms)
f = open("src/tir-print.c", "w")
f.write(output)
f.close()
