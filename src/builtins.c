/**
 * @file builtins.c
 * @brief Definitions of builtin functions for the source language and their
 * assembly code.
 * @author Milan Novák
 */

#include "builtins.h"

/*
 * AI-GENERATED CODE
 * Tool/model: Claude Sonnet 5.5 (Anthropic, claude.ai chat)
 * Scope: only the .assembler_code strings of the builtins in this file.
 *
 * The assembly code for the builtin functions was written with AI. The model
 recieved the
 * context of the specification of IFJCode26 and the required functionality of
 the builtin
 * functions.
 *
 * Prompt: "add assembler code for the builtin functions print, readLine,
 readInt, readDouble, readBoolean, toStr, toInt, strLength, subStr, ord, chr,
 listDrop, listTake, listSlice, listHead, listTail, listToString, listToList.
    Funciton calling convention: the callee pushes the parameters to the data
 stack from left to right. The functon must always return a value. The value of
 unit is represented as nil. If the function uses local variables, create a
 stackframe. Conventions: Instructions in uppercase, function names:
 functionName$0, all labels must be prefixed with the function label string. Do
 not implement any additional functionality or functions. Do not make arbitrary
 decisions, always ask for clarification."
 */

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
                      "JUMPIFEQ print$0_ARRAY LF@type string@array\n"
                      "WRITE LF@val\n"
                      "JUMP print$0_END\n"
                      "LABEL print$0_UNIT\n"
                      "WRITE string@()\n"
                      "JUMP print$0_END\n"
                      "LABEL print$0_ARRAY\n"
                      "PUSHS LF@val\n"
                      "CALL toStr$0\n"
                      "POPS LF@val\n"
                      "WRITE LF@val\n"
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
                      "JUMPIFEQ toStr$0_ARRAY LF@type string@array\n"
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
                      "JUMP toStr$0_END\n"
                      "LABEL toStr$0_ARRAY\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@val int@0\n"
                      "MOVE LF@res string@List(\n"
                      "DEFVAR LF@i\n"
                      "MOVE LF@i int@1\n"
                      "DEFVAR LF@elem\n"
                      "DEFVAR LF@cond\n"
                      "LABEL toStr$0_LOOP\n"
                      "GT LF@cond LF@i LF@len\n"
                      "JUMPIFEQ toStr$0_LOOP_END LF@cond bool@true\n"
                      "JUMPIFEQ toStr$0_NOSEP LF@i int@1\n"
                      "CONCAT LF@res LF@res string@,\\032\n"
                      "LABEL toStr$0_NOSEP\n"
                      "ARRAYGET LF@elem LF@val LF@i\n"
                      "PUSHS LF@elem\n"
                      "CALL toStr$0\n"
                      "POPS LF@elem\n"
                      "CONCAT LF@res LF@res LF@elem\n"
                      "ADD LF@i LF@i int@1\n"
                      "JUMP toStr$0_LOOP\n"
                      "LABEL toStr$0_LOOP_END\n"
                      "CONCAT LF@res LF@res string@)\n"
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
    .assembler_code = "LABEL list$drop$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@n\n"
                      "POPS LF@n\n"
                      "DEFVAR LF@list\n"
                      "POPS LF@list\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@list int@0\n"
                      "DEFVAR LF@cond\n"
                      "LT LF@cond LF@n int@0\n"
                      "JUMPIFEQ list$drop$0_LOWOK LF@cond bool@false\n"
                      "MOVE LF@n int@0\n"
                      "LABEL list$drop$0_LOWOK\n"
                      "GT LF@cond LF@n LF@len\n"
                      "JUMPIFEQ list$drop$0_HIGHOK LF@cond bool@false\n"
                      "MOVE LF@n LF@len\n"
                      "LABEL list$drop$0_HIGHOK\n"
                      "DEFVAR LF@newlen\n"
                      "SUB LF@newlen LF@len LF@n\n"
                      "DEFVAR LF@size\n"
                      "ADD LF@size LF@newlen int@1\n"
                      "DEFVAR LF@res\n"
                      "NEWARRAY LF@res LF@size\n"
                      "ARRAYSET LF@res int@0 LF@newlen\n"
                      "DEFVAR LF@i\n"
                      "MOVE LF@i int@0\n"
                      "DEFVAR LF@src\n"
                      "DEFVAR LF@dst\n"
                      "DEFVAR LF@elem\n"
                      "LABEL list$drop$0_LOOP\n"
                      "JUMPIFEQ list$drop$0_END LF@i LF@newlen\n"
                      "ADD LF@src LF@n LF@i\n"
                      "ADD LF@src LF@src int@1\n"
                      "ADD LF@dst LF@i int@1\n"
                      "ARRAYGET LF@elem LF@list LF@src\n"
                      "ARRAYSET LF@res LF@dst LF@elem\n"
                      "ADD LF@i LF@i int@1\n"
                      "JUMP list$drop$0_LOOP\n"
                      "LABEL list$drop$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature listTake = {
    .name = "list$take",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 2U,
    .parameters = {TYPE_ANY_LIST, TYPE_INT},
    .assembler_code = "LABEL list$take$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@n\n"
                      "POPS LF@n\n"
                      "DEFVAR LF@list\n"
                      "POPS LF@list\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@list int@0\n"
                      "DEFVAR LF@cond\n"
                      "LT LF@cond LF@n int@0\n"
                      "JUMPIFEQ list$take$0_LOWOK LF@cond bool@false\n"
                      "MOVE LF@n int@0\n"
                      "LABEL list$take$0_LOWOK\n"
                      "GT LF@cond LF@n LF@len\n"
                      "JUMPIFEQ list$take$0_HIGHOK LF@cond bool@false\n"
                      "MOVE LF@n LF@len\n"
                      "LABEL list$take$0_HIGHOK\n"
                      "DEFVAR LF@size\n"
                      "ADD LF@size LF@n int@1\n"
                      "DEFVAR LF@res\n"
                      "NEWARRAY LF@res LF@size\n"
                      "ARRAYSET LF@res int@0 LF@n\n"
                      "DEFVAR LF@i\n"
                      "MOVE LF@i int@0\n"
                      "DEFVAR LF@dst\n"
                      "DEFVAR LF@elem\n"
                      "LABEL list$take$0_LOOP\n"
                      "JUMPIFEQ list$take$0_END LF@i LF@n\n"
                      "ADD LF@dst LF@i int@1\n"
                      "ARRAYGET LF@elem LF@list LF@dst\n"
                      "ARRAYSET LF@res LF@dst LF@elem\n"
                      "ADD LF@i LF@i int@1\n"
                      "JUMP list$take$0_LOOP\n"
                      "LABEL list$take$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature listSlice = {
    .name = "list$slice",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 3U,
    .parameters = {TYPE_ANY_LIST, TYPE_INT, TYPE_INT},
    .assembler_code = "LABEL list$slice$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@until\n"
                      "POPS LF@until\n"
                      "DEFVAR LF@from\n"
                      "POPS LF@from\n"
                      "DEFVAR LF@list\n"
                      "POPS LF@list\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@list int@0\n"
                      "DEFVAR LF@cond\n"
                      "LT LF@cond LF@from int@0\n"
                      "JUMPIFEQ list$slice$0_FROMOK LF@cond bool@false\n"
                      "MOVE LF@from int@0\n"
                      "LABEL list$slice$0_FROMOK\n"
                      "GT LF@cond LF@until LF@len\n"
                      "JUMPIFEQ list$slice$0_UNTILOK LF@cond bool@false\n"
                      "MOVE LF@until LF@len\n"
                      "LABEL list$slice$0_UNTILOK\n"
                      "DEFVAR LF@newlen\n"
                      "SUB LF@newlen LF@until LF@from\n"
                      "LT LF@cond LF@newlen int@0\n"
                      "JUMPIFEQ list$slice$0_LENOK LF@cond bool@false\n"
                      "MOVE LF@newlen int@0\n"
                      "LABEL list$slice$0_LENOK\n"
                      "DEFVAR LF@size\n"
                      "ADD LF@size LF@newlen int@1\n"
                      "DEFVAR LF@res\n"
                      "NEWARRAY LF@res LF@size\n"
                      "ARRAYSET LF@res int@0 LF@newlen\n"
                      "DEFVAR LF@i\n"
                      "MOVE LF@i int@0\n"
                      "DEFVAR LF@src\n"
                      "DEFVAR LF@dst\n"
                      "DEFVAR LF@elem\n"
                      "LABEL list$slice$0_LOOP\n"
                      "JUMPIFEQ list$slice$0_END LF@i LF@newlen\n"
                      "ADD LF@src LF@from LF@i\n"
                      "ADD LF@src LF@src int@1\n"
                      "ADD LF@dst LF@i int@1\n"
                      "ARRAYGET LF@elem LF@list LF@src\n"
                      "ARRAYSET LF@res LF@dst LF@elem\n"
                      "ADD LF@i LF@i int@1\n"
                      "JUMP list$slice$0_LOOP\n"
                      "LABEL list$slice$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature listHead = {
    .name = "list$head",
    .return_type = TYPE_ANY,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY_LIST},
    .assembler_code = "LABEL list$head$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@list\n"
                      "POPS LF@list\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@list int@0\n"
                      "JUMPIFNEQ list$head$0_OK LF@len int@0\n"
                      "EXIT int@40\n"
                      "LABEL list$head$0_OK\n"
                      "DEFVAR LF@res\n"
                      "ARRAYGET LF@res LF@list int@1\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature listTail = {
    .name = "list$tail",
    .return_type = TYPE_ANY_LIST,
    .parameter_count = 1U,
    .parameters = {TYPE_ANY_LIST},
    .assembler_code = "LABEL list$tail$0\n"
                      "CREATEFRAME\n"
                      "PUSHFRAME\n"
                      "DEFVAR LF@list\n"
                      "POPS LF@list\n"
                      "DEFVAR LF@len\n"
                      "ARRAYGET LF@len LF@list int@0\n"
                      "JUMPIFNEQ list$tail$0_OK LF@len int@0\n"
                      "EXIT int@40\n"
                      "LABEL list$tail$0_OK\n"
                      "DEFVAR LF@newlen\n"
                      "SUB LF@newlen LF@len int@1\n"
                      "DEFVAR LF@size\n"
                      "ADD LF@size LF@newlen int@1\n"
                      "DEFVAR LF@res\n"
                      "NEWARRAY LF@res LF@size\n"
                      "ARRAYSET LF@res int@0 LF@newlen\n"
                      "DEFVAR LF@i\n"
                      "MOVE LF@i int@0\n"
                      "DEFVAR LF@src\n"
                      "DEFVAR LF@dst\n"
                      "DEFVAR LF@elem\n"
                      "LABEL list$tail$0_LOOP\n"
                      "JUMPIFEQ list$tail$0_END LF@i LF@newlen\n"
                      "ADD LF@src LF@i int@2\n"
                      "ADD LF@dst LF@i int@1\n"
                      "ARRAYGET LF@elem LF@list LF@src\n"
                      "ARRAYSET LF@res LF@dst LF@elem\n"
                      "ADD LF@i LF@i int@1\n"
                      "JUMP list$tail$0_LOOP\n"
                      "LABEL list$tail$0_END\n"
                      "PUSHS LF@res\n"
                      "POPFRAME\n"
                      "RETURN\n"};

static const BuiltinSignature listToString = {.name = "list$toString",
                                              .return_type = TYPE_STRING,
                                              .parameter_count = 1U,
                                              .parameters = {TYPE_ANY_LIST},
                                              .assembler_code =
                                                  "LABEL list$toString$0\n"
                                                  "CALL toStr$0\n"
                                                  "RETURN\n"};

static const BuiltinSignature listToList = {.name = "list$toList",
                                            .return_type = TYPE_ANY_LIST,
                                            .parameter_count = 1U,
                                            .parameters = {TYPE_ANY_LIST},
                                            .assembler_code =
                                                "LABEL list$toList$0\n"
                                                "RETURN\n"};

const BuiltinSignature BUILTINS[] = {
    print,    readLine,  readInt,  readDouble, readBoolean,  toStr,
    toInt,    strLength, subStr,   ord,        chr,          listDrop,
    listTake, listSlice, listHead, listTail,   listToString, listToList};

const size_t BUILTIN_COUNT = sizeof(BUILTINS) / sizeof(BUILTINS[0]);