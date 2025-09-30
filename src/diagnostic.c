#include "diagnostic.h"

#include "lex.h"
#include "tir.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>

static int find_line_num(String source, SourceIndex where) {
    int line_num = 1;

    for (int32_t i = 0; i < where.index; i++) {
        if (source.ptr[i] == '\n') {
            line_num++;
        }
    }

    return line_num;
}

static void print_term(TirContext c, TirId term) {
    TirTag tag = get_term_tag(c, term);

    if (is_tir_type(tag)) {
        print_type(stderr, c, term);
        return;
    }

    if (is_tir_value(tag)) {
        fprintf(stderr, "value of type ");
        print_type(stderr, c, get_value_type(c, term));
        return;
    }

    if (tag == TIR_MODULE) {
        fprintf(stderr, "module");
        return;
    }

    if (tag == TIR_GENERIC) {
        fprintf(stderr, "generic");
        return;
    }

    fprintf(stderr, "{error}");
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
    } else if (diagnostic->kind < NOTE_END) {
        color = "\033[96;1m";
    } else {
        color = "\033[35;1m";
    }

    fprintf(stderr, "\033[0;1m%s:%d:%d: ", loc->path, line_num, (int) (loc->where.index - line_start.index + 1));
    fprintf(stderr, "\033[0m%s", color);

    if (diagnostic->kind < ERROR_END) {
        fprintf(stderr, "error[E%04d]\033[0m: ", diagnostic->kind);
    } else if (diagnostic->kind < NOTE_END) {
        fprintf(stderr, "note\033[0m: ");
    } else {
        fprintf(stderr, "warning\033[0m: ");
    }

    #define CASE(name, id, ...) case DIAGNOSTIC_##name: { Diagnostic##name id = diagnostic->Diagnostic##name; __VA_ARGS__ }
    switch (diagnostic->kind) {
        case ERROR_END:
        case NOTE_END:
        case WARNING_END: {
            abort();
        }
        CASE(ErrorInvalidToken, d, {
            (void) d;
            fprintf(stderr, "invalid token");
            break;
        })
        CASE(ErrorExpectedToken, d, {
            fprintf(stderr, "expected %s", token_tag_to_string(d.expected));
            break;
        })
        CASE(ErrorExpectedExpression, d, {
            (void) d;
            fprintf(stderr, "expected expression");
            break;
        })
        CASE(ErrorExpectedDefinition, d, {
            (void) d;
            fprintf(stderr, "expected definition");
            break;
        })
        CASE(ErrorInvalidTokenAfterExtern, d, {
            fprintf(
                stderr,
                "expected function or mut, but found %s",
                token_tag_to_string(d.provided)
            );
            break;
        })
        CASE(ErrorEmptyChar, d, {
            (void) d;
            fprintf(stderr, "empty character literal");
            break;
        })
        CASE(ErrorCharTooLong, d, {
            (void) d;
            fprintf(stderr, "character literal too long");
            break;
        })
        CASE(ErrorEscapeSequence, d, {
            (void) d;
            fprintf(stderr, "unknown escape sequence");
            break;
        })
        CASE(ErrorUnterminatedString, d, {
            (void) d;
            fprintf(stderr, "unterminated double quote string");
            break;
        })
        CASE(ErrorInvalidFloat, d, {
            (void) d;
            fprintf(stderr, "invalid float literal");
            break;
        })
        CASE(ErrorRecursion, d, {
            (void) d;
            fprintf(stderr, "recursive dependency");
            break;
        })
        CASE(ErrorExpectedValue, d, {
            (void) d;
            fprintf(stderr, "expected value");
            break;
        })
        CASE(ErrorExpectedType, d, {
            (void) d;
            fprintf(stderr, "expected type");
            break;
        })
        CASE(ErrorMultipleDefinition, d, {
            (void) d;
            fprintf(stderr, "name is defined multiple times");
            break;
        })
        CASE(ErrorMultipleExternDefinition, d, {
            (void) d;
            fprintf(stderr, "extern symbol is defined multiple times");
            break;
        })
        CASE(ErrorUndefinedModule, d, {
            (void) d;
            fprintf(stderr, "unknown module");
            break;
        })
        CASE(ErrorUndefinedName, d, {
            (void) d;
            fprintf(stderr, "use of undefined name");
            break;
        })
        CASE(ErrorUndefinedNameFromModule, d, {
            (void) d;
            fprintf(stderr, "module does not contain such an item");
            break;
        })
        CASE(ErrorDerefOperandRole, d, {
            (void) d;
            fprintf(stderr, "expected value or type");
            break;
        })
        CASE(ErrorAccessOperandRole, d, {
            fprintf(stderr, "expected value, type or module, but found ");
            print_term(d.ctx, d.provided);
            break;
        })
        CASE(ErrorCallOperandRole, d, {
            fprintf(stderr, "expected value or type, but found ");
            print_term(d.ctx, d.provided);
            break;
        })
        CASE(ErrorIndexOperandRole, d, {
            (void) d;
            fprintf(stderr, "expected value, type or macro");
            break;
        })
        CASE(ErrorEnumExpectsIntType, d, {
            fprintf(stderr, "enum layout type must be an integer type, but found ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorArrayTypeExpectsLengthType, d, {
            fprintf(stderr, "array index type must be `ArrayLength, but found ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorUnaryUnexpectedOperand, d, {
            fprintf(stderr, "cannot apply unary operator to type ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorBinaryUnexpectedOperands, d, {
            fprintf(stderr, "cannot apply binary operator to types ");
            print_type(stderr, d.ctx, d.type1);
            fprintf(stderr, " and ");
            print_type(stderr, d.ctx, d.type2);
            break;
        })
        CASE(ErrorDerefUnexpectedOperand, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " cannot be dereferenced");
            break;
        })
        CASE(ErrorCast, d, {
            fprintf(stderr, "cannot cast from ");
            print_type(stderr, d.ctx, d.type1);
            fprintf(stderr, " to ");
            print_type(stderr, d.ctx, d.type2);
            break;
        })
        CASE(ErrorSliceCtorExpectsPointer, d, {
            fprintf(stderr, "slice data field must be a pointer, but found ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorTypeConstructorType, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " does not have a constructor");
            break;
        })
        CASE(ErrorCallee, d, {
            fprintf(stderr, "expected function, but found ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorArgumentCount, d, {
            int32_t param_count = get_function_type(d.ctx, d.type).param_count;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                param_count,
                param_count == 1 ? "argument" : "arguments",
                d.provided
            );
            break;
        })
        CASE(ErrorTypeArgumentInference, d, {
            (void) d;
            fprintf(stderr, "couldn't infer type arguments");
            break;
        })
        CASE(ErrorFieldCount, d, {
            int32_t field_count = get_struct_type(d.ctx, d.type).field_count;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                field_count,
                field_count == 1 ? "field" : "fields",
                d.provided
            );
            break;
        })
        CASE(ErrorAffineCtorCount, d, {
            (void) d;
            fprintf(stderr, "expected 1 field");
            break;
        })
        CASE(ErrorIndexCount, d, {
            fprintf(stderr, "expected 1 index, but provided %"PRIi32, d.provided);
            break;
        })
        CASE(ErrorWrongCount, d, {
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                d.expected,
                d.expected == 1 ? "argument" : "arguments",
                d.provided
            );
            break;
        })
        CASE(ErrorTaggedTypeWrongCount, d, {
            int32_t param_count = get_generic_term(d.ctx, d.type).type_count;
            fprintf(
                stderr,
                "expected %"PRIi32" %s, but provided %"PRIi32,
                param_count,
                param_count == 1 ? "type argument" : "type arguments",
                d.provided
            );
            break;
        })
        CASE(ErrorIndexOperand, d, {
            fprintf(stderr, "expected array or slice, but found ");
            print_type(stderr, d.ctx, d.type);
            break;
        })
        CASE(ErrorUndefinedTypeScope, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " does not have such an item");
            break;
        })
        CASE(ErrorUndefinedTypeField, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " does not have such a field");
            break;
        })
        CASE(ErrorExpectedValueType, d, {
            fprintf(stderr, "expected ");
            print_type(stderr, d.ctx, d.expected);
            fprintf(stderr, ", but found ");
            print_type(stderr, d.ctx, d.provided);
            break;
        })
        CASE(ErrorExpectedMutablePlace, d, {
            (void) d;
            fprintf(stderr, "cannot assign to this expression");
            break;
        })
        CASE(ErrorConstInit, d, {
            (void) d;
            fprintf(stderr, "initializer is not a constant expression");
            break;
        })
        CASE(ErrorConstIntOverflow, d, {
            (void) d;
            fprintf(stderr, "integer overflow");
            break;
        })
        CASE(ErrorConstNegativeShift, d, {
            (void) d;
            fprintf(stderr, "can't shift by a negative integer");
            break;
        })
        CASE(ErrorTypeInference, d, {
            (void) d;
            fprintf(stderr, "can't infer type");
            break;
        })
        CASE(ErrorTypeUnknownTypeSize, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " has unknown size");
            break;
        })
        CASE(ErrorTypeUnknownTypeAlignment, d, {
            fprintf(stderr, "type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " has unknown alignment requirements");
            break;
        })
        CASE(ErrorIndexUnknownTypeSize, d, {
            fprintf(stderr, "cannot index array of type ");
            print_type(stderr, d.ctx, d.type);
            fprintf(stderr, " because it has unknown size at compile time");
            break;
        })
        CASE(ErrorEmptyArray, d, {
            (void) d;
            fprintf(stderr, "empty array");
            break;
        })
        CASE(ErrorEmptyStruct, d, {
            (void) d;
            fprintf(stderr, "empty struct");
            break;
        })
        CASE(ErrorSwitchIncompatibleCases, d, {
            (void) d;
            fprintf(stderr, "switch arms have incompatible types");
            break;
        })
        CASE(ErrorMisplacedBreak, d, {
            (void) d;
            fprintf(stderr, "break outside of loop");
            break;
        })
        CASE(ErrorMisplacedContinue, d, {
            (void) d;
            fprintf(stderr, "continue outside of loop");
            break;
        })
        CASE(ErrorReturnMissingValue, d, {
            (void) d;
            fprintf(stderr, "returning no value from a function with return type");
            break;
        })
        CASE(ErrorReturnExpectedValue, d, {
            (void) d;
            fprintf(stderr, "returning value from a function with no return type");
            break;
        })
        CASE(ErrorMissingReturn, d, {
            (void) d;
            fprintf(stderr, "no value returned from function with return type");
            break;
        })
        CASE(ErrorMainSignature, d, {
            (void) d;
            fprintf(stderr, "main function must take no arguments and return nothing");
            break;
        })
        CASE(ErrorAffineAssignment, d, {
            (void) d;
            fprintf(stderr, "cannot assign to affine type");
            break;
        })
        CASE(ErrorConsumedValueUsed, d, {
            (void) d;
            fprintf(stderr, "use of consumed variable");
            break;
        })
        CASE(ErrorConsumedInLoop, d, {
            (void) d;
            fprintf(stderr, "variable is consumed in a loop");
            break;
        })
        CASE(ErrorMoveBorrowed, d, {
            (void) d;
            fprintf(stderr, "cannot move a variable while it is borrowed");
            break;
        })
        CASE(ErrorBorrowedMutableShared, d, {
            (void) d;
            fprintf(stderr, "cannot have a mutable and shared reference at the same time");
            break;
        })
        CASE(ErrorMultipleMutableBorrows, d, {
            (void) d;
            fprintf(stderr, "can only have one mutable reference at any given time");
            break;
        })
        CASE(ErrorDuplicateSwitchCase, d, {
            (void) d;
            fprintf(stderr, "duplicate switch case");
            break;
        })
        CASE(ErrorElseCaseUnreachable, d, {
            (void) d;
            fprintf(stderr, "else case is unreachable");
            break;
        })
        CASE(ErrorSwitchNotExhaustive, d, {
            (void) d;
            fprintf(stderr, "switch must cover all possible values");
            break;
        })
        CASE(NoteReplaceLetWithMut, d, {
            (void) d;
            fprintf(stderr, "consider replacing `let` with `mut`");
            break;
        })
        CASE(NotePreviousDefinition, d, {
            (void) d;
            fprintf(stderr, "previous definition");
            break;
        })
        CASE(NotePreviousBuiltinDefinition, d, {
            (void) d;
            fprintf(stderr, "a built-in with the name already exists");
            break;
        })
        CASE(NotePrivateDefinition, d, {
            (void) d;
            fprintf(stderr, "definition is private");
            break;
        })
        CASE(NoteForgotImport, d, {
            (void) d;
            fprintf(stderr, "did you forget to import module?");
            break;
        })
        CASE(NoteRecursion, d, {
            (void) d;
            fprintf(stderr, "recursion happens here");
            break;
        })
        CASE(WarningUnused, d, {
            (void) d;
            fprintf(stderr, "unused name, add a leading _ to remove this warning");
            break;
        })
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
}
