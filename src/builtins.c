#include "builtins.h"

// TODO mark assembler code as AI written

// TODO add support for arrays in type dependant functions
static const BuiltinSignature print = {
    .name = "print",
    .return_type = TYPE_UNIT,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY},
    .assembler_code = "LABEL print$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@val\n"
                      "JUMPIFEQ print$0_UNIT LF@type string@nil\n"
                      "WRITE LF@val\n"
                      "JUMP print$0_END\n"
                      "LABEL print$0_UNIT\n"
                      "WRITE string@()\n"
                      "LABEL print$0_END\n"
                      "PUSHS nil@nil\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readLine = {
    .name = "readLine",
    .return_type = TYPE_STRING,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readLine$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res string\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ readLine$0_OK LF@type string@nil\n"
                      "MOVE LF@res string@\n"
                      "LABEL readLine$0_OK\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readInt = {
    .name = "readInt",
    .return_type = TYPE_INT,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readInt$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res int\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ readInt$0_OK LF@type string@nil\n"
                      "MOVE LF@res int@0\n"
                      "LABEL readInt$0_OK\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readDouble = {
    .name = "readDouble",
    .return_type = TYPE_REAL,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readDouble$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res float\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ readDouble$0_OK LF@type string@nil\n"
                      "MOVE LF@res float@0x0p+0\n"
                      "LABEL readDouble$0_OK\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature readBoolean = {
    .name = "readBoolean",
    .return_type = TYPE_BOOL,
    .parameter_count = 0U,
    .parameters = {TYPE_UNKNOWN},
    .assembler_code = "LABEL readBoolean$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@res\n"
                      "READ LF@res bool\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@res\n"
                      "JUMPIFNEQ readBoolean$0_OK LF@type string@nil\n"
                      "MOVE LF@res bool@false\n"
                      "LABEL readBoolean$0_OK\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature toStr = {
    .name = "toStr",
    .return_type = TYPE_STRING,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY},
    .assembler_code = "LABEL toStr$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@val\n"
                      "POPS LF@val\n"
                      "DEFVAR LF@type\n"
                      "TYPE LF@type LF@val\n"
                      "DEFVAR LF@res\n"
                      "JUMPIFEQ toStr$0_INT LF@type string@int\n"
                      "JUMPIFEQ toStr$0_FLOAT LF@type string@float\n"
                      "JUMPIFEQ toStr$0_BOOL LF@type string@bool\n"
                      "JUMPIFEQ toStr$0_UNIT LF@type string@nil\n"
                      "MOVE LF@res LF@val\n"
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_INT\n"
                      "INT2STR LF@res LF@val\n"
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_FLOAT\n"
                      "FLOAT2STR LF@res LF@val\n"
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_BOOL\n"
                      "JUMPIFEQ toStr$0_TRUE LF@val bool@true\n"
                      "MOVE LF@res string@false\n"
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_TRUE\n"
                      "MOVE LF@res string@true\n"
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_UNIT\n"
                      "MOVE LF@res string@()\n"
                      "LABEL toStr$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature toInt = {.name = "toInt",
                                       .return_type = TYPE_INT,
                                       .parameter_count = 1U,
                                       .parameters = {TYPE_REAL},
                                       .assembler_code =
                                           "LABEL toInt$0\n"
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
                                               "LABEL strLength$0\n"
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
    .assembler_code = "LABEL subStr$0\n"
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
                      "JUMPIFEQ subStr$0_ERR LF@cond bool@true\n"
                      "LT LF@cond LF@end int@0\n"
                      "JUMPIFEQ subStr$0_ERR LF@cond bool@true\n"
                      "GT LF@cond LF@start LF@end\n"
                      "JUMPIFEQ subStr$0_ERR LF@cond bool@true\n"
                      "LT LF@cond LF@start LF@len\n"
                      "JUMPIFEQ subStr$0_ERR LF@cond bool@false\n"
                      "GT LF@cond LF@end LF@len\n"
                      "JUMPIFEQ subStr$0_ERR LF@cond bool@true\n"
                      "DEFVAR LF@res\n"
                      "MOVE LF@res string@\n"
                      "DEFVAR LF@ch\n"
                      "LABEL subStr$0_LOOP\n"
                      "JUMPIFEQ subStr$0_END LF@start LF@end\n"
                      "GETCHAR LF@ch LF@val LF@start\n"
                      "CONCAT LF@res LF@res LF@ch\n"
                      "ADD LF@start LF@start int@1\n"
                      "JUMP subStr$0_LOOP\n"
                      "LABEL subStr$0_ERR\n"
                      "EXIT int@40\n"
                      "LABEL subStr$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature ord = {
    .name = "ord",
    .return_type = TYPE_INT,
    .parameter_count = 2U,
    .parameters = {TYPE_STRING, TYPE_INT},
    .assembler_code = "LABEL ord$0\n"
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
                      "JUMPIFEQ ord$0_BAD LF@cond bool@true\n"
                      "LT LF@cond LF@idx LF@len\n"
                      "JUMPIFEQ ord$0_BAD LF@cond bool@false\n"
                      "STRI2INT LF@res LF@val LF@idx\n"
                      "JUMP ord$0_END\n"
                      "LABEL ord$0_BAD\n"
                      "MOVE LF@res int@-1\n"
                      "LABEL ord$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature chr = {.name = "chr",
                                     .return_type = TYPE_STRING,
                                     .parameter_count = 1U,
                                     .parameters = {TYPE_INT},
                                     .assembler_code =
                                         "LABEL chr$0\n"
                                         "CREATEFRAME\n"
                                         "PUSHFRAME\n"
                                         "DEFVAR LF@code\n"
                                         "POPS LF@code\n"
                                         "DEFVAR LF@res\n"
                                         "INT2CHAR LF@res LF@code\n"
                                         "PUSHS LF@res\n"
                                         "POPFRAME\n"
                                         "RETURN\n"};

static const BuiltinSignature listDrop = {
    .name = "list$drop",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 2U,
    .parameters = {TYPE_ANY_LIST, TYPE_INT},
    .assembler_code = ""};

static const BuiltinSignature listTake = {
    .name = "list$take",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 2U,
    .parameters = {TYPE_ANY_LIST, TYPE_INT},
    .assembler_code = ""};

static const BuiltinSignature listSlice = {
    .name = "list$slice",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 3U,
    .parameters = {TYPE_ANY_LIST, TYPE_INT, TYPE_INT},
    .assembler_code = ""};

static const BuiltinSignature listHead = {.name = "list$head",
                                          .return_type = TYPE_ANY,
                                          .parameter_count = 1U,
                                          .parameters = {TYPE_ANY_LIST},
                                          .assembler_code = ""};

static const BuiltinSignature listTail = {.name = "list$tail",
                                          .return_type = TYPE_ANY_LIST,
                                          .parameter_count = 1U,
                                          .parameters = {TYPE_ANY_LIST},
                                          .assembler_code = ""};

static const BuiltinSignature listToString = {.name = "list$toString",
                                              .return_type = TYPE_STRING,
                                              .parameter_count = 1U,
                                              .parameters = {TYPE_ANY_LIST},
                                              .assembler_code = ""};

static const BuiltinSignature listToList = {.name = "list$toList",
                                            .return_type = TYPE_ANY_LIST,
                                            .parameter_count = 1U,
                                            .parameters = {TYPE_ANY_LIST},
                                            .assembler_code = ""};

const BuiltinSignature BUILTINS[] = {
    print,    readLine,  readInt,  readDouble, readBoolean,  toStr,
    toInt,    strLength, subStr,   ord,        chr,          listDrop,
    listTake, listSlice, listHead, listTail,   listToString, listToList};

const size_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);