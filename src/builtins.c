#include "builtins.h"

// TODO mark assembler code as AI written

static const BuiltinSignature print = {
    .name = "print",
    .return_type = TYPE_UNIT,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY},
    .assembler_code = "LABEL print\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@val\n"
                      "JUMPIFEQ $print_unit LF@type string@nil\n"
                      "WRITE LF@val\n"
                      "JUMP $print_end\n"
                      "LABEL $print_unit\n"
                      "WRITE string@()\n"
                      "LABEL $print_end\n"
                      "PUSHS nil@nil\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readLine = {
    .name = "readLine",
    .return_type = TYPE_STRING,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readLine\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res string\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ $readLine_ok LF@type string@nil\n"
                      "MOVE LF@res string@\n"
                      "LABEL $readLine_ok\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readInt = {
    .name = "readInt",
    .return_type = TYPE_INT,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readInt\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res int\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ $readInt_ok LF@type string@nil\n"
                      "MOVE LF@res int@0\n"
                      "LABEL $readInt_ok\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readDouble = {
    .name = "readDouble",
    .return_type = TYPE_REAL,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readDouble\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res float\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ $readDouble_ok LF@type string@nil\n"
                      "MOVE LF@res float@0x0p+0\n"
                      "LABEL $readDouble_ok\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readBoolean = {
    .name = "readBoolean",
    .return_type = TYPE_BOOL,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readBoolean\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res bool\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ $readBoolean_ok LF@type string@nil\n"
                      "MOVE LF@res bool@false\n"
                      "LABEL $readBoolean_ok\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature toStr = {
    .name = "toStr",
    .return_type = TYPE_STRING,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY},
    .assembler_code = "LABEL toStr\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@val\n"
                      "DEFVAR LF@res\n"
                      "JUMPIFEQ $toStr_int LF@type string@int\n"
                      "JUMPIFEQ $toStr_float LF@type string@float\n"
                      "JUMPIFEQ $toStr_bool LF@type string@bool\n"
                      "JUMPIFEQ $toStr_unit LF@type string@nil\n"
                      "MOVE LF@res LF@val\n"
                      "JUMP $toStr_end\n"
                      "LABEL $toStr_int\n"
                      "INT2STR LF@res LF@val\n"
                      "JUMP $toStr_end\n"
                      "LABEL $toStr_float\n"
                      "FLOAT2STR LF@res LF@val\n"
                      "JUMP $toStr_end\n"
                      "LABEL $toStr_bool\n"
                      "JUMPIFEQ $toStr_true LF@val bool@true\n"
                      "MOVE LF@res string@false\n"
                      "JUMP $toStr_end\n"
                      "LABEL $toStr_true\n"
                      "MOVE LF@res string@true\n"
                      "JUMP $toStr_end\n"
                      "LABEL $toStr_unit\n"
                      "MOVE LF@res string@()\n"
                      "LABEL $toStr_end\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature toInt = {.name = "toInt",
                                       .return_type = TYPE_INT,
                                       .parameter_count = 1U,
                                       .parameters = {TYPE_REAL},
                                       .assembler_code =
                                           "LABEL toInt\n"
                                           "CREATEFRAME\n"
                                           "PUSHFRAME\n"
                                           "DEFVAR LF@val\n"
                                           "POPS LF@val\n"
                                           "DEFVAR LF@res\n"
                                           "FLOAT2INT LF@res LF@val\n"
                                           "PUSHS LF@res\n"
                                           "POPFRAME\n"
                                           "RETURN\n"};

static const BuiltinSignature strLength = {.name = "strLength",
                                           .return_type = TYPE_INT,
                                           .parameter_count = 1U,
                                           .parameters = {TYPE_STRING},
                                           .assembler_code =
                                               "LABEL strLength\n"
                                               "CREATEFRAME\n"
                                               "PUSHFRAME\n"
                                               "DEFVAR LF@val\n"
                                               "POPS LF@val\n"
                                               "DEFVAR LF@res\n"
                                               "STRLEN LF@res LF@val\n"
                                               "PUSHS LF@res\n"
                                               "POPFRAME\n"
                                               "RETURN\n"};

static const BuiltinSignature subStr = {
    .name = "subStr",
    .return_type = TYPE_STRING,
    .parameter_count = 3U,
    .parameters = {TYPE_STRING, TYPE_INT, TYPE_INT},
    .assembler_code = "LABEL subStr\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@end\n"
                      "POPS LF@end\n"
                      "DEFVAR LF@start\n"
                      "POPS LF@start\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@len\n"
                      "STRLEN LF@len LF@val\n"
                      "DEFVAR LF@cond\n"
                      "LT LF@cond LF@start int@0\n"
                      "JUMPIFEQ $subStr_err LF@cond bool@true\n"
                      "LT LF@cond LF@end int@0\n"
                      "JUMPIFEQ $subStr_err LF@cond bool@true\n"
                      "GT LF@cond LF@start LF@end\n"
                      "JUMPIFEQ $subStr_err LF@cond bool@true\n"
                      "LT LF@cond LF@start LF@len\n"
                      "JUMPIFEQ $subStr_err LF@cond bool@false\n"
                      "GT LF@cond LF@end LF@len\n"
                      "JUMPIFEQ $subStr_err LF@cond bool@true\n"
                      "DEFVAR LF@res\n"
                      "MOVE LF@res string@\n"
                      "DEFVAR LF@ch\n"
                      "LABEL $subStr_loop\n"
                      "JUMPIFEQ $subStr_end LF@start LF@end\n"
                      "GETCHAR LF@ch LF@val LF@start\n"
                      "CONCAT LF@res LF@res LF@ch\n"
                      "ADD LF@start LF@start int@1\n"
                      "JUMP $subStr_loop\n"
                      "LABEL $subStr_err\n"
                      "EXIT int@40\n"
                      "LABEL $subStr_end\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature ord = {
    .name = "ord",
    .return_type = TYPE_INT,
    .parameter_count = 2U,
    .parameters = {TYPE_STRING, TYPE_INT},
    .assembler_code = "LABEL ord\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@idx\n"
                      "POPS LF@idx\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@res\n"
                      "DEFVAR LF@len\n"
                      "STRLEN LF@len LF@val\n"
                      "DEFVAR LF@cond\n"
                      "LT LF@cond LF@idx int@0\n"
                      "JUMPIFEQ $ord_bad LF@cond bool@true\n"
                      "LT LF@cond LF@idx LF@len\n"
                      "JUMPIFEQ $ord_bad LF@cond bool@false\n"
                      "STRI2INT LF@res LF@val LF@idx\n"
                      "JUMP $ord_end\n"
                      "LABEL $ord_bad\n"
                      "MOVE LF@res int@-1\n"
                      "LABEL $ord_end\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature chr = {.name = "chr",
                                     .return_type = TYPE_STRING,
                                     .parameter_count = 1U,
                                     .parameters = {TYPE_INT},
                                     .assembler_code =
                                         "LABEL chr\n"
                                         "CREATEFRAME\n"
                                         "PUSHFRAME\n"
                                         "DEFVAR LF@code\n"
                                         "POPS LF@code\n"
                                         "DEFVAR LF@res\n"
                                         "INT2CHAR LF@res LF@code\n"
                                         "PUSHS LF@res\n"
                                         "POPFRAME\n"
                                         "RETURN\n"};

const BuiltinSignature BUILTINS[] = {
    print, readLine,  readInt, readDouble, readBoolean, toStr,
    toInt, strLength, subStr,  ord,        chr,
};

const size_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);