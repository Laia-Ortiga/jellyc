
# This file generates only the creation and access of tree nodes.
# Everything else can be "safely" done in C through the generated functions.


class Type:
    def __init__(self, c_type, count = None, is_list = None, c_print = None):
        self.c_type = c_type
        self.count = count or 1
        self.is_list = is_list or False
        self.c_print = c_print


node = Type("AstId {}")
file = Type("FileId {}")
ty = Type("TirId {}")
val = Type("TirId {}")
token = Type("SourceIndex {}")
strtab = Type("int32_t {}")
scope = Type("int32_t {}", c_print="%d")
i32 = Type("int32_t {}", c_print="%d")
i64 = Type("int64_t {}", count=2, c_print="%ld")
f64 = Type("double {}", count=2, c_print="%f")
boolean = Type("int32_t {}", c_print="%d")


def list_of(T):
    return Type(
        "struct {{ int32_t len; " + T.c_type.format("*ptr") + "; }} {}",
        is_list=T,
    )


ast_nodes = [
    {
        "name": "root",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "defs", "ty": list_of(node) },
        ],
    },
    {
        "name": "import",
        "fields": [
            { "name": "token", "ty": token },
        ],
    },
    {
        "name": "public",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "def", "ty": node },
        ],
    },
    {
        "name": "function",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "type_params", "ty": list_of(node) },
            { "name": "params", "ty": list_of(node) },
            { "name": "ret", "ty": node },
            { "name": "body", "ty": node },
        ],
    },
    {
        "name": "enum",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "repr", "ty": node },
            { "name": "members", "ty": list_of(node) },
        ],
    },
    {
        "name": "struct",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "has_public_fields", "ty": boolean },
            { "name": "type_params", "ty": list_of(node) },
            { "name": "fields", "ty": list_of(node) },
        ],
    },
    {
        "name": "newtype",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "type_params", "ty": list_of(node) },
            { "name": "type", "ty": node },
        ],
    },
    {
        "name": "const",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "init", "ty": node },
        ],
    },
    {
        "name": "extern_function",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "params", "ty": list_of(node) },
            { "name": "ret", "ty": node },
        ],
    },
    {
        "name": "extern_var",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "type", "ty": node },
        ],
    },
    {
        "name": "param",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "type", "ty": node },
        ],
    },
    {
        "name": "let",
        "names": [
            "let",
            "mut",
        ],
        "fields": [
            { "name": "token", "ty": token },
            { "name": "init", "ty": node },
        ],
    },
    {
        "name": "if",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "condition", "ty": node },
            { "name": "true_block", "ty": node },
            { "name": "false_block", "ty": node },
        ],
    },
    {
        "name": "while",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "condition", "ty": node },
            { "name": "block", "ty": node },
        ],
    },
    {
        "name": "for",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "init", "ty": node },
            { "name": "condition", "ty": node },
            { "name": "next", "ty": node },
            { "name": "block", "ty": node },
        ],
    },
    {
        "name": "switch",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "condition", "ty": node },
            { "name": "branches", "ty": list_of(node) },
        ],
    },
    {
        "name": "switch_case",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "pattern", "ty": node },
            { "name": "value", "ty": node },
        ],
    },
    {
        "name": "break",
        "fields": [
            { "name": "token", "ty": token },
        ],
    },
    {
        "name": "continue",
        "fields": [
            { "name": "token", "ty": token },
        ],
    },
    {
        "name": "return",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "value", "ty": node },
        ],
    },
    {
        "name": "array_type",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "index", "ty": node },
            { "name": "elem", "ty": node },
        ],
    },
    {
        "name": "array_type_sugar",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "length", "ty": node },
            { "name": "elem", "ty": node },
        ],
    },
    {
        "name": "unary",
        "names": [
            "ptr_type",
            "mut_ptr_type",
            "slice_type",
            "mut_slice_type",
            "plus",
            "minus",
            "not",
            "address",
            "deref",
        ],
        "fields": [
            { "name": "token", "ty": token },
            { "name": "a", "ty": node },
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
            "logic_and",
            "logic_or",
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
            { "name": "token", "ty": token },
            { "name": "a", "ty": node },
            { "name": "b", "ty": node },
        ],
    },
    {
        "name": "function_type",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "params", "ty": list_of(node) },
            { "name": "ret", "ty": node },
        ],
    },
    {
        "name": "type_hint",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "type", "ty": node },
            { "name": "value", "ty": node },
        ],
    },
    {
        "name": "call",
        "names": [
            "call",
            "index",
            "slice",
        ],
        "fields": [
            { "name": "token", "ty": token },
            { "name": "a", "ty": node },
            { "name": "args", "ty": list_of(node) },
        ],
    },
    {
        "name": "access",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "s", "ty": node },
        ],
    },
    {
        "name": "inferred_access",
        "fields": [
            { "name": "token", "ty": token },
        ],
    },
    {
        "name": "list",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "elems", "ty": list_of(node) },
        ],
    },
    {
        "name": "map_entry",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "value", "ty": node },
        ],
    },
    {
        "name": "map",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "entries", "ty": list_of(node) },
        ],
    },
    {
        "name": "block",
        "fields": [
            { "name": "token", "ty": token },
            { "name": "stmts", "ty": list_of(node) },
        ],
    },
    {
        "name": "leaf",
        "names": [
            "id",
            "int",
            "float",
            "char",
            "string",
            "true",
            "false",
            "null",
        ],
        "fields": [
            { "name": "token", "ty": token },
        ],
    },
]


others_terms = [
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
            { "name": "has_public_fields", "ty": boolean },
            { "name": "file", "ty": file },
            { "name": "fields", "ty": list_of(ty) },

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
            "cast",
            "checked_cast",
            "unsigned_cast",
            "array_to_slice",
        ],
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "a", "ty": val },
        ],
    },
    {
        "name": "size_of",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "operand_type", "ty": ty },
        ],
    },
    {
        "name": "align_of",
        "fields": [
            { "name": "node", "ty": node },
            { "name": "type", "ty": ty },
            { "name": "operand_type", "ty": ty },
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
            { "name": "field_indices", "ty": list_of(i32) },
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

output = ''
indent = 0

def add_line(x):
    global output
    global indent
    diff = x.count('{') - x.count('}')
    if diff < 0:
        indent += diff
    output += '    ' * indent
    output += x
    output += '\n'
    if diff > 0:
        indent += diff

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
    add_line("{}_{},".format(module.upper(), name.upper()))

def gen_header(path, barriers, variants, config):
    global output
    module = config["module"]
    output = ''

    add_line("typedef struct {")
    add_line("int32_t private_field_id;")
    add_line("} " + to_pascal(module) + "Id;")
    add_line("")

    add_line("typedef struct {")
    for i in range(config["data_size"]):
        add_line("int32_t " + chr(i + ord('a')) + ";")
    add_line("} " + to_pascal(module) + "Data;")
    add_line("")

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
            add_line(f["ty"].c_type.format(f["name"]) + ";")
        add_line("}} {};".format(to_pascal(module) + to_pascal(name)))
        add_line("")

    # Generate creation functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" in v:
            add_line(to_pascal(module)
                + "Id "
                + module + "_push_" + v["name"]
                + "("
                + config["context_type"].format("c")
                + ", "
                + to_pascal(module) + "Tag tag, "
                + to_pascal(module) + to_pascal(v["name"])
                + " a);"
            )
        else:
            add_line(to_pascal(module)
                + "Id "
                + module + "_push_" + v["name"]
                + "("
                + config["context_type"].format("c")
                + ", " + to_pascal(module) + to_pascal(v["name"]) + " a);"
            )

    add_line("")

    # Generate access functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        add_line(to_pascal(module) + to_pascal(name)
            + " "
            + module + "_get_" + name
            + "("
            + config["context_type"].format("c")
            + ", "
            + to_pascal(module)
            + "Id a);"
        )

    add_line("")
    add_line("#define " + module + "_push(c, ...) \\")
    add_line("    (_Generic((__VA_ARGS__), \\")
    lines = []
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" not in v:
            name = v["name"]
            lines.append("        "
                + to_pascal(module) + to_pascal(name)
                + ": "
                + module + "_push_" + name)
    for i, line in enumerate(lines):
        if i == len(lines) - 1:
            add_line(line + " \\")
        else:
            add_line(line + ", \\")
    add_line("    )(c, __VA_ARGS__))\n")

    add_line("#define " + module + "_push_tag(c, tag, ...) \\")
    add_line("    (_Generic((__VA_ARGS__), \\")
    lines = []
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        if "names" in v:
            name = v["name"]
            lines.append("        "
                + to_pascal(module) + to_pascal(name)
                + ": "
                + module + "_push_" + name)
    for i, line in enumerate(lines):
        if i == len(lines) - 1:
            add_line(line + " \\")
        else:
            add_line(line + ", \\")
    add_line("    )(c, tag, __VA_ARGS__))")
    add_line('')
    for b in barriers:
        if b[0] is None:
            continue
        add_line('#define ' + module.upper() + '_{}_START {}'.format(b[0], b[1]))
        add_line('#define ' + module.upper() + '_{}_END {}'.format(b[0], b[2]))

    f = open(path, "w")
    f.write(output[:-1])
    f.close()


def gen_source(path, variants, config):
    global output
    module = config["module"]
    data_size = config["data_size"]
    output = '''#include "{}.h"

#include <stdlib.h>

'''.format(module)
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
                + "("
                + config["context_type"].format("c")
                + ", "
                + to_pascal(module) + "Tag tag, "
                + type_name + " a) {"
            )
            add_line("switch (tag) {")
            for subname in (v.get("names") or [name]):
                add_line("case {}_{}:".format(module.upper(), subname.upper()))
            add_line("    break;")
            add_line("default:")
            add_line("    abort();")
            add_line("}")
        else:
            add_line(to_pascal(module)
                + "Id "
                + module
                + "_push_"
                + name
                + "("
                + config["context_type"].format("c")
                + ", "
                + type_name
                + " a) {"
            )
        add_line(to_pascal(module) + "Data data;")

        needs_extra = False
        min_count = 0
        expanded = []
        lists = []

        for field in v["fields"]:
            if field["ty"].is_list:
                needs_extra = True
                expanded.append({
                    "name": field["name"] + ".len",
                    "ty": i32,
                    "step": 0,
                })
                lists.append({
                    "name": field["name"],
                    "ty": field["ty"],
                })
            else:
                for step in range(field["ty"].count):
                    expanded.append({
                        "name": field["name"],
                        "ty": field["ty"],
                        "step": step,
                    })
            min_count += field["ty"].count

        if min_count > data_size:
            needs_extra = True

        max_data_size = data_size
        index = 0
        if needs_extra:
            max_data_size -= 1

        remaining_fields = []
        for field in expanded:
            if index >= max_data_size:
                remaining_fields.append(field)
                continue
            add_line("memcpy(&data.{}, (int32_t *) &a.{} + {}, sizeof(int32_t));".format(chr(index + ord('a')), field["name"], field["step"]))
            index += 1

        if needs_extra:
            add_line("int32_t n = {};".format(len(remaining_fields)))
            for field in lists:
                add_line("n += a.{}.len * (sizeof(a.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))

            add_line("data.{} = {}extra.len;".format(chr(max_data_size + ord('a')), config["writer"]))
            add_line("int32_t *extra = vec_grow(&" + config["writer"] + "extra, n);")

            for field in remaining_fields:
                add_line("memcpy(extra++, (int32_t *) &a.{} + {}, sizeof(int32_t));".format(field["name"], field["step"]))

            for field in lists:
                add_line("memcpy(extra, a.{}.ptr, a.{}.len * sizeof(a.{}.ptr[0]));".format(field["name"], field["name"], field["name"]))
                add_line("extra += a.{}.len * (sizeof(a.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))

        if "names" in v:
            add_line("return new_" + module + "(c, tag, data);")
        else:
            add_line("return new_" + module + "(c, {}_{}, data);".format(module.upper(), name.upper()))
        add_line("}")
        add_line("")

    # Generate access functions.
    for v in variants:
        if len(v["fields"]) == 0:
            continue
        name = v["name"]
        add_line(to_pascal(module) + to_pascal(name)
            + " " + module + "_get_" + name
            + "("
            + config["context_type"].format("c")
            + ", "
            + to_pascal(module) + "Id a) {")
        add_line("switch (get_" + module +"_tag(c, a)) {")
        for subname in (v.get("names") or [name]):
            add_line("case {}_{}:".format(module.upper(), subname.upper()))
        add_line("    break;")
        add_line("default:")
        add_line("    abort();")
        add_line("}")

        add_line("{} result;".format(to_pascal(module) + to_pascal(name)))

        needs_extra = False
        min_count = 0
        expanded = []
        lists = []

        for field in v["fields"]:
            if field["ty"].is_list:
                needs_extra = True
                expanded.append({
                    "name": field["name"] + ".len",
                    "ty": i32,
                    "step": 0,
                })
                lists.append({
                    "name": field["name"],
                    "ty": field["ty"],
                })
            else:
                for step in range(field["ty"].count):
                    expanded.append({
                        "name": field["name"],
                        "ty": field["ty"],
                        "step": step,
                    })
            min_count += field["ty"].count

        if min_count > data_size:
            needs_extra = True

        max_data_size = data_size
        index = 0
        if needs_extra:
            max_data_size -= 1

        remaining_fields = []
        for field in expanded:
            if index >= max_data_size:
                remaining_fields.append(field)
                continue
            add_line("memcpy((int32_t *) &result.{} + {}, &{}{}, sizeof(int32_t));".format(field["name"], field["step"], config["main_access"], chr(index + ord('a'))))
            index += 1

        if needs_extra:
            add_line("int32_t *extra = {}extra.ptr + {}{};".format(config["extra_access"], config["main_access"], chr(max_data_size + ord('a'))))

            for field in remaining_fields:
                add_line("memcpy((int32_t *) &result.{} + {}, extra++, sizeof(int32_t));".format(field["name"], field["step"]))

            for field in lists:
                add_line("result.{}.ptr = (void *) extra;".format(field["name"]))
                add_line("extra += result.{}.len * (sizeof(result.{}.ptr[0]) / sizeof(int32_t));".format(field["name"], field["name"]))

        add_line("return result;")
        add_line("}")
        add_line("")

    f = open(path, "w")
    f.write(output[:-1])
    f.close()

def gen_ast_print():
    global output
    output = ''
    add_line("static void print_ast_node(AstPrinter *p, AstId a) {")
    add_line("switch (get_ast_tag(p->ast, a)) {")
    for v in ast_nodes:
        name = v["name"]
        type_name = "Ast" + to_pascal(v["name"])
        for subname in (v.get("names") or [name]):
            add_line("case AST_" + subname.upper() + ": {")
            lexer_idx = 0

            if name == "root":
                add_line("printf(\"Root\");")
            elif len(v["fields"]) > 0:
                add_line(type_name + " t = ast_get_" + name + "(p->ast, a);")
                add_line("p->depth++;")
                add_line("printf(\"" + to_pascal(subname) + "(\\n\");")

                for field in v["fields"]:
                    if field["ty"] == token:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": \");")
                        add_line("Lexer lexer{} = new_lexer(substring(p->source, t.".format(lexer_idx) + field["name"] + ".index, p->source.len));")
                        add_line("Token token = next_token(&lexer{});".format(lexer_idx))
                        add_line("String s = substring(lexer{}.source, token.start.index, token.end.index);".format(lexer_idx))
                        lexer_idx += 1
                        add_line("fwrite(s.ptr, 1, s.len, stdout);")
                        add_line("printf(\",\\n\");")
                    elif field["ty"] == node:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": \");")
                        add_line("print_ast_node(p, t." + field["name"] + ");")
                        add_line("printf(\",\\n\");")
                    elif field["ty"].is_list:
                        add_line("print_indent(p->depth++);")
                        add_line("printf(\"" + field["name"] + ": [\\n\");")
                        add_line("for (int32_t i = 0; i < t." + field["name"] + ".len; i++) {")
                        add_line("print_indent(p->depth);")
                        add_line("print_ast_node(p, t." + field["name"] + ".ptr[i]);")
                        add_line("printf(\",\\n\");")
                        add_line("}")
                        add_line("print_indent(--p->depth);")
                        add_line("printf(\"],\\n\");")

                add_line("print_indent(--p->depth);")
                add_line("printf(\")\");")
            else:
                add_line("printf(\"" + to_pascal(subname) + "\");")
            add_line("break;")
            add_line("}")
    add_line("}")
    add_line("}")
    f = open("src/ast-print.c", "w")
    f.write(output)
    f.close()

def gen_tir_print():
    global output
    output = ''
    add_line("static void print_tir_node(TirPrinter *p, TirId a) {")
    add_line("switch (get_tir_tag(p->tir, a)) {")
    for v in terms:
        name = v["name"]
        type_name = "Tir" + to_pascal(v["name"])
        for subname in (v.get("names") or [name]):
            add_line("case TIR_" + subname.upper() + ": {")

            if len(v["fields"]) > 0:
                add_line(type_name + " t = tir_get_" + name + "(p->tir, a);")
                add_line("p->depth++;")
                add_line("printf(\"" + to_pascal(subname) + "(\\n\");")

                for field in v["fields"]:
                    if field["ty"] == ty:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": \");")
                        add_line("print_type(stdout, p->tir, t." + field["name"] + ");")
                        add_line("printf(\",\\n\");")
                    elif field["ty"] == val:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": \");")
                        add_line("print_tir_node(p, t." + field["name"] + ");")
                        add_line("printf(\",\\n\");")
                    elif field["ty"] == strtab:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": %s,\\n\", tir_get_str(p->tir, t." + field["name"] + "));")
                    elif field["ty"].c_print is not None:
                        add_line("print_indent(p->depth);")
                        add_line("printf(\"" + field["name"] + ": " + field["ty"].c_print + ",\\n\", t." + field["name"] + ");")
                    elif field["ty"].is_list == ty or field["ty"].is_list == val:
                        add_line("print_indent(p->depth++);")
                        add_line("printf(\"" + field["name"] + ": [\\n\");")
                        add_line("for (int32_t i = 0; i < t." + field["name"] + ".len; i++) {")
                        add_line("print_indent(p->depth);")
                        add_line("print_tir_node(p, t." + field["name"] + ".ptr[i]);")
                        add_line("printf(\",\\n\");")
                        add_line("}")
                        add_line("print_indent(--p->depth);")
                        add_line("printf(\"],\\n\");")

                add_line("print_indent(--p->depth);")
                add_line("printf(\")\");")
            else:
                add_line("printf(\"" + to_pascal(subname) + "\\n\");")
            add_line("break;")
            add_line("}")
    add_line("}")
    add_line("}")
    f = open("src/tir-print.c", "w")
    f.write(output)
    f.close()


# Ast

ast_barriers, ast = join_sets(
    {"name": None, "set": ast_nodes},
)
ast_config = {
    "module": "ast",
    "data_size": 3,
    "context_type": "Ast *{}",
    "writer": "c->",
    "main_access": "nth(c->nodes.data_table, a).",
    "extra_access": "c->",
}
gen_header("src/ast-types.h", ast_barriers, ast, ast_config)
gen_source("src/ast-types.c", ast, ast_config)
gen_ast_print()

# Tir

tir_barriers, terms = join_sets(
    {"name": None, "set": others_terms},
    {"name": "TYPE", "set": types},
    {"name": "VALUE", "set": values},
)
tir_config = {
    "module": "tir",
    "data_size": 4,
    "context_type": "TirContext {}",
    "writer": "tir_writer(c)->terms.",
    "main_access": "get_term_data(c, a)->",
    "extra_access": "tir_get_storage(c, a).tir->terms.",
}
gen_header("src/tir-types.h", tir_barriers, terms, tir_config)
gen_source("src/tir-types.c", terms, tir_config)
gen_tir_print()
