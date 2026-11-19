#include "instructions.h"
#include <stddef.h>

const InstructionOperand TRASH_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "trash"}}};

const InstructionOperand LIST_GLOBAL_RES = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_res"}}};

const InstructionOperand LIST_INDEX_GLOBAL_RES = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_index_res"}}};

const InstructionOperand LIST_ELEMENT_GLOBAL_RES = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_element_res"}}};

const InstructionOperand LIST_SIZE_GLOBAL_RES = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_size_res"}}};

const InstructionOperand LIST_GLOBAL_A = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_a"}}};

const InstructionOperand LIST_INDEX_GLOBAL_A = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_index_a"}}};

const InstructionOperand LIST_ELEMENT_GLOBAL_A = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_element_a"}}};

const InstructionOperand LIST_SIZE_GLOBAL_A = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_size_a"}}};

const InstructionOperand LIST_GLOBAL_B = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_b"}}};

const InstructionOperand LIST_INDEX_GLOBAL_B = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_index_b"}}};

const InstructionOperand LIST_ELEMENT_GLOBAL_B = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_element_b"}}};

const InstructionOperand LIST_SIZE_GLOBAL_B = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "list_size_b"}}};

const InstructionOperand FROM_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "from"}}};

const InstructionOperand TO_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "to"}}};

const InstructionOperand BY_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "by"}}};

const InstructionOperand COND_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "cond"}}};

const InstructionOperand TMP_GLOBAL = {
    .type = COMPILER_VAR_TYPE,
    .value = {.compiler_variable = {.frame = GF, .name = "tmp"}}};

const InstructionOperand GLOBAL_VARS[] = {
    TRASH_GLOBAL,
    LIST_GLOBAL_A,
    LIST_INDEX_GLOBAL_A,
    LIST_ELEMENT_GLOBAL_A,
    LIST_SIZE_GLOBAL_A,
    LIST_GLOBAL_B,
    LIST_INDEX_GLOBAL_B,
    LIST_ELEMENT_GLOBAL_B,
    LIST_SIZE_GLOBAL_B,
    LIST_GLOBAL_RES,
    LIST_INDEX_GLOBAL_RES,
    LIST_ELEMENT_GLOBAL_RES,
    LIST_SIZE_GLOBAL_RES,
    FROM_GLOBAL,
    TO_GLOBAL,
    BY_GLOBAL,
    COND_GLOBAL,
    TMP_GLOBAL,
};

const size_t GLOBAL_VAR_COUNT = sizeof(GLOBAL_VARS) / sizeof(GLOBAL_VARS[0]);