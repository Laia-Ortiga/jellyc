import re
import sys

import xml.etree.ElementTree as et

tree = et.parse(sys.argv[1])

print('module gl')
print()
print('public extern function gladLoadGLLoader(loadproc function(name *char) -> *mut byte) -> i32')
print()

root = tree.getroot()
functions = set()
enums = set()
type_map = {
    "GLenum": "i32",
    "GLboolean": "bool",
    "GLbitfield": "i32",
    "GLbyte": "i8",
    "GLshort": "i16",
    "GLint": "i32",
    "GLubyte": "i8",
    "GLushort": "i16",
    "GLuint": "i32",
    "GLsizei": "i32",
    "GLfloat": "f32",
    "GLclampf": "f32",
    "GLdouble": "f64",
    "GLclampd": "f64",
    "GLchar": "char",
    "GLintptr": "isize",
    "GLsizeiptr": "isize",
    "GLint64": "i64",
    "GLuint64": "i64",
    "GLsync": "*mut byte",
    "GLDEBUGPROC": "function(source i32, type i32, id i32, severity i32, length i32, message *char, userParam *mut byte)",
}

for feature in root.iter('feature'):
    if feature.attrib['api'] != 'gl':
        continue
    for c in feature.iter('command'):
        functions.add(c.attrib['name'])
    for e in feature.iter('enum'):
        enums.add(e.attrib['name'])

for commands in root.iter('commands'):
    for command in commands:
        proto = command.find('proto')
        name = proto.find('name').text
        if name not in functions:
            continue
        params = []
        for param in command.iter('param'):
            pname = param.find('name')
            ptype = param.find('ptype')
            param_name = param.find('name').text
            if param_name == "pointer":
                param_name += '_'
            if ptype is None:
                type_nospace = param.text.replace("const", "").replace(" ", "").replace("void*", "")
                params.append(param_name + ' ' + type_nospace + '*mut byte')
            else:
                is_const = param.text == "const "
                prefix = ptype.tail.replace(" ", "").replace("const*", "!")
                if not is_const:
                    prefix = prefix.replace("*", "*mut ")
                prefix = prefix.replace("!", "*")
                params.append(param_name + ' ' + prefix + type_map[ptype.text])
        ret_type = proto.find('ptype')
        ret_type_str = ""
        if ret_type is not None:
            ret_type_str = ' -> ' + type_map[ret_type.text]
        elif proto.text != 'void ':
            ret_type_str = ' -> *mut byte'
        print("public extern mut glad_", name, " function(", ", ".join(params), ")", ret_type_str, sep='')

print()

for groups in root.iter('enums'):
    for e in groups.iter('enum'):
        name = e.attrib['name']
        if name not in enums:
            continue
        if name[3].isdigit():
            name = '_' + name[3:]
        else:
            name = name[3:]
        print("public const", name, "=", e.attrib['value'], "as i32")
