#include "diagnostic.h"

#include "lex.h"
#include "data/tir.h"

#include <omp.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _OPENMP
static omp_lock_t print_lock;
#endif

void init_diagnostic_module(void) {
#ifdef _OPENMP
    omp_init_lock(&print_lock);
#endif
}

static int find_line_num(String source, SourceIndex where) {
    int line_num = 1;

    for (int32_t i = 0; i < where.index; i++) {
        if (source.ptr[i] == '\n') {
            line_num++;
        }
    }

    return line_num;
}

void print_diagnostic(SourceLoc const *loc, Diagnostic const *diagnostic) {
    SourceIndex line_start = loc->where;

    while (line_start.index > 0 && loc->source.ptr[line_start.index - 1] != '\n') {
        line_start.index--;
    }

    SourceIndex line_end = loc->where;

    while (line_end.index < loc->source.len && loc->source.ptr[line_end.index] != '\n') {
        line_end.index++;
    }

    int line_num = find_line_num(loc->source, line_start);
    char const *color = "";

    if (diagnostic->kind < ERROR_END) {
        color = "\033[31;1m";
    } else {
        color = "\033[96;1m";
    }

#ifdef _OPENMP
    omp_set_lock(&print_lock);
#endif

    fprintf(stderr, "\033[0;1m%s:%d:%d: ", loc->path, line_num, (int) (loc->where.index - line_start.index + 1));
    fprintf(stderr, "\033[0m%s", color);

    if (diagnostic->kind < ERROR_END) {
        fprintf(stderr, "error[E%04d]\033[0m: ", diagnostic->kind);
    } else {
        fprintf(stderr, "note\033[0m: ");
    }

    switch (diagnostic->kind) {
        case ERROR_END:
        case NOTE_END: {
            abort();
        }
        case ERROR_INVALID_TOKEN: {
            fprintf(stderr, "invalid token");
            break;
        }
        case ERROR_EXPECTED_TOKEN: {
            fprintf(stderr, "expected %s", token_tag_to_string(diagnostic->expected_token));
            break;
        }
        case ERROR_EXPECTED_EXPRESSION: {
            fprintf(stderr, "expected expression");
            break;
        }
        case ERROR_EXPECTED_DEFINITION: {
            fprintf(stderr, "expected definition");
            break;
        }
        case ERROR_INVALID_TOKEN_AFTER_EXTERN: {
            fprintf(stderr, "expected function or mut, but found %s", token_tag_to_string(diagnostic->expected_token));
            break;
        }
        case ERROR_EMPTY_CHAR: {
            fprintf(stderr, "empty character literal");
            break;
        }
        case ERROR_MULTIPLE_CHAR: {
            fprintf(stderr, "multiple character literal");
            break;
        }
        case ERROR_ESCAPE_SEQUENCE: {
            fprintf(stderr, "unknown escape sequence");
            break;
        }
        case ERROR_UNTERMINATED_STRING: {
            fprintf(stderr, "unterminated double quote string");
            break;
        }
        case ERROR_RECURSIVE_DEPENDENCY: {
            fprintf(stderr, "recursive dependency");
            break;
        }
        case ERROR_EXPECTED_VALUE: {
            fprintf(stderr, "expected value");
            break;
        }
        case ERROR_EXPECTED_TYPE: {
            fprintf(stderr, "expected type");
            break;
        }
        case ERROR_MULTIPLE_DEFINITION: {
            fprintf(stderr, "name is defined multiple times");
            break;
        }
        case ERROR_MULTIPLE_EXTERN_DEFINITION: {
            fprintf(stderr, "extern symbol is defined multiple times");
            break;
        }
        case ERROR_UNDEFINED_MODULE: {
            fprintf(stderr, "unknown module");
            break;
        }
        case ERROR_UNDEFINED_NAME: {
            fprintf(stderr, "use of undefined name");
            break;
        }
        case ERROR_UNDEFINED_NAME_FROM_MODULE: {
            fprintf(stderr, "module does not contain such an item");
            break;
        }
        case ERROR_DEREF_OPERAND_ROLE: {
            fprintf(stderr, "expected value or type");
            break;
        }
        case ERROR_ACCESS_OPERAND_ROLE: {
            fprintf(stderr, "expected value, type or module");
            break;
        }
        case ERROR_CALL_OPERAND_ROLE: {
            fprintf(stderr, "expected value or type");
            break;
        }
        case ERROR_INDEX_OPERAND_ROLE: {
            fprintf(stderr, "expected value, type or macro");
            break;
        }
        case ERROR_ENUM_EXPECTS_INT_TYPE: {
            fprintf(stderr, "enum layout type must be an integer type, but found ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_ARRAY_TYPE_EXPECTS_LENGTH_TYPE: {
            fprintf(stderr, "array index type must be `ArrayLength, but found ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_UNARY_UNEXPECTED_OPERAND: {
            fprintf(stderr, "cannot apply unary operator to type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_BINARY_UNEXPECTED_OPERANDS: {
            fprintf(stderr, "cannot apply binary operator to types ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type1);
            fprintf(stderr, " and ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type2);
            break;
        }
        case ERROR_DEREF_UNEXPECTED_OPERAND: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " cannot be dereferenced");
            break;
        }
        case ERROR_CAST: {
            fprintf(stderr, "cannot cast from ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type1);
            fprintf(stderr, " to ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type2);
            break;
        }
        case ERROR_SLICE_CTOR_EXPECTS_POINTER: {
            fprintf(stderr, "slice data field must be a pointer, but found ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_TYPE_CONSTRUCTOR_TYPE: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " does not have a constructor");
            break;
        }
        case ERROR_CALLEE: {
            fprintf(stderr, "expected function, but found ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_ARGUMENT_COUNT: {
            int32_t param_count = get_function_type(diagnostic->type_error.ctx, diagnostic->type_error.type).param_count;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                param_count,
                param_count == 1 ? "argument" : "arguments",
                diagnostic->type_error.extra
            );
            break;
        }
        case ERROR_TYPE_ARGUMENT_INFERENCE: {
            fprintf(stderr, "couldn't infer type arguments");
            break;
        }
        case ERROR_FIELD_COUNT: {
            int32_t field_count = get_struct_type(diagnostic->type_error.ctx, diagnostic->type_error.type).field_count;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                field_count,
                field_count == 1 ? "field" : "fields",
                diagnostic->type_error.extra
            );
            break;
        }
        case ERROR_LINEAR_CTOR_COUNT: {
            fprintf(stderr, "expected 1 field");
            break;
        }
        case ERROR_INDEX_COUNT: {
            fprintf(stderr, "expected 1 index, but provided %"PRIi32, diagnostic->type_error.extra);
            break;
        }
        case ERROR_WRONG_COUNT: {
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                diagnostic->count_error.expected,
                diagnostic->count_error.expected == 1 ? "argument" : "arguments",
                diagnostic->count_error.provided
            );
            break;
        }
        case ERROR_TAGGED_TYPE_WRONG_COUNT: {
            int32_t param_count = get_newtype_type(diagnostic->type_error.ctx, diagnostic->type_error.type).tags;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                param_count,
                param_count == 1 ? "type argument" : "type arguments",
                diagnostic->type_error.extra
            );
            break;
        }
        case ERROR_INDEX_OPERAND: {
            fprintf(stderr, "expected array or slice, but found ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            break;
        }
        case ERROR_UNDEFINED_TYPE_SCOPE: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " does not have such an item");
            break;
        }
        case ERROR_UNDEFINED_TYPE_FIELD: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " does not have such a field");
            break;
        }
        case ERROR_EXPECTED_VALUE_TYPE: {
            fprintf(stderr, "expected ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type1);
            fprintf(stderr, ", but found ");
            print_type(stderr, diagnostic->double_type_error.ctx, diagnostic->double_type_error.type2);
            break;
        }
        case ERROR_EXPECTED_MUTABLE_PLACE: {
            fprintf(stderr, "cannot assign to this expression");
            break;
        }
        case ERROR_CONST_INIT: {
            fprintf(stderr, "initializer is not a constant expression");
            break;
        }
        case ERROR_CONST_INT_OVERFLOW: {
            fprintf(stderr, "integer overflow");
            break;
        }
        case ERROR_CONST_NEGATIVE_SHIFT: {
            fprintf(stderr, "can't shift by a negative integer");
            break;
        }
        case ERROR_TYPE_INFERENCE: {
            fprintf(stderr, "can't infer type");
            break;
        }
        case ERROR_TYPE_UNKNOWN_TYPE_SIZE: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " has unknown size");
            break;
        }
        case ERROR_TYPE_UNKNOWN_TYPE_ALIGNMENT: {
            fprintf(stderr, "type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " has unknown alignment requirements");
            break;
        }
        case ERROR_INDEX_UNKNOWN_TYPE_SIZE: {
            fprintf(stderr, "cannot index array of type ");
            print_type(stderr, diagnostic->type_error.ctx, diagnostic->type_error.type);
            fprintf(stderr, " because it has unknown size at compile time");
            break;
        }
        case ERROR_EMPTY_ARRAY: {
            fprintf(stderr, "empty array");
            break;
        }
        case ERROR_EMPTY_STRUCT: {
            fprintf(stderr, "empty struct");
            break;
        }
        case ERROR_SWITCH_INCOMPATIBLE_CASES: {
            fprintf(stderr, "switch arms have incompatible types");
            break;
        }
        case ERROR_MISPLACED_BREAK: {
            fprintf(stderr, "break outside of loop");
            break;
        }
        case ERROR_MISPLACED_CONTINUE: {
            fprintf(stderr, "continue outside of loop");
            break;
        }
        case ERROR_RETURN_MISSING_VALUE: {
            fprintf(stderr, "returning no value from a function with return type");
            break;
        }
        case ERROR_RETURN_EXPECTED_VALUE: {
            fprintf(stderr, "returning value from a function with no return type");
            break;
        }
        case ERROR_MISSING_RETURN: {
            fprintf(stderr, "no value returned from function with return type");
            break;
        }
        case ERROR_MAIN_SIGNATURE: {
            fprintf(stderr, "main function must take no arguments and return nothing");
            break;
        }
        case ERROR_LINEAR_ASSIGNMENT: {
            fprintf(stderr, "cannot assign to linear type");
            break;
        }
        case ERROR_CONSUMED_VALUE_USED: {
            fprintf(stderr, "use of consumed variable");
            break;
        }
        case ERROR_CONSUMED_IN_LOOP: {
            fprintf(stderr, "variable is consumed in a loop");
            break;
        }
        case ERROR_MOVE_BORROWED: {
            fprintf(stderr, "cannot move a variable while it is borrowed");
            break;
        }
        case ERROR_BORROWED_MUTABLE_SHARED: {
            fprintf(stderr, "cannot have a mutable and shared reference at the same time");
            break;
        }
        case ERROR_MULTIBLE_MUTABLE_BORROWS: {
            fprintf(stderr, "can only have one mutable reference at any given time");
            break;
        }
        case ERROR_DUPLICATE_SWITCH_CASE: {
            fprintf(stderr, "duplicate switch case");
            break;
        }
        case ERROR_ELSE_CASE_UNREACHABLE: {
            fprintf(stderr, "else case is unreachable");
            break;
        }
        case ERROR_SWITCH_NOT_EXHAUSTIVE: {
            fprintf(stderr, "switch must cover all possible values");
            break;
        }
        case NOTE_REPLACE_LET_WITH_MUT: {
            fprintf(stderr, "consider replacing `let` with `mut`");
            break;
        }
        case NOTE_PREVIOUS_DEFINITION: {
            fprintf(stderr, "previous definition");
            break;
        }
        case NOTE_PREVIOUS_BUILTIN_DEFINITION: {
            fprintf(stderr, "a built-in with the name already exists");
            break;
        }
        case NOTE_PRIVATE_DEFINITION: {
            fprintf(stderr, "definition is private");
            break;
        }
        case NOTE_FORGOT_IMPORT: {
            fprintf(stderr, "did you forget to import module?");
            break;
        }
        case NOTE_RECURSION: {
            fprintf(stderr, "recursion happens here");
            break;
        }
    }

    fprintf(stderr, "\n");
    int indent = fprintf(stderr, "%d | ", line_num);
    fprintf(stderr, "%.*s\n%s", (int) (line_end.index - line_start.index), &loc->source.ptr[line_start.index], color);

    for (int i = 0; i < indent; i++) {
        fputc(' ', stderr);
    }

    for (SourceIndex i = line_start; i.index < line_end.index; i.index++) {
        char c = ' ';

        if (i.index == loc->mark.index) {
            c = '^';
        } else if (i.index >= loc->where.index && i.index < loc->where.index + loc->len) {
            c = '~';
        }

        fputc(c, stderr);
    }

    fputs("\033[0m\n", stderr);

#ifdef _OPENMP
    omp_unset_lock(&print_lock);
#endif
}
