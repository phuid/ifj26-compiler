/*
 * AI-generated test block — black-box conformance harness for IFJ26.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   Write a header-only helper for spec conformance tests. ifj26_exit_code
 *   runs a whole IFJ26 program through lexer, p_program and semantic_check
 *   and maps the statuses to the exit codes of spec chapter 6. Prepend the
 *   mandatory prolog from one constant unless IFJ26_HARNESS_NO_PROLOG=1.
 *   Run each program in a forked child with a time limit so that a crash or
 *   a hang in the compiler fails one test instead of the whole binary.
 */
#ifndef TESTS_SPEC_HARNESS_H_
#define TESTS_SPEC_HARNESS_H_

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/semantic.h"
}

/*
 * Status -> exit code mapping (spec chapter 6), checked against src/*.h:
 *
 *   ParserStatus (parser.h)        exit code
 *   PARSER_OK                      -> run semantic_check, see below
 *   PARSER_LEXER_ERROR             -> 1   (every LexerStatus error is folded
 *                                          into this one by the parser)
 *   PARSER_SYNTAX_ERROR            -> 2
 *   PARSER_SEMANTIC_ERROR          -> IFJ26_PARSER_SEMANTIC (-3): the parser
 *                                     uses one status for an undefined
 *                                     variable (3), a redefinition (4), a
 *                                     parameter of @main (5) and a type
 *                                     conflict (6), so no single code of
 *                                     chapter 6 can be derived from it
 *   PARSER_INVALID_ARGUMENT        -> 99
 *   PARSER_INTERNAL_ERROR          -> 99
 *
 *   SemanticStatus (semantic.h)    the enum values already equal the codes:
 *   SEMANTIC_OK 0, SEMANTIC_UNDEFINED 3, SEMANTIC_REDEFINITION 4,
 *   SEMANTIC_BAD_CALL 5, SEMANTIC_TYPE_ERROR 6, SEMANTIC_OTHER 7,
 *   SEMANTIC_INTERNAL_ERROR 99. Code 10 has no enum value.
 *
 * Harness-only results (never valid exit codes of the compiler):
 *   IFJ26_CRASH   (-1)  the compiler was killed by a signal (e.g. SIGSEGV)
 *   IFJ26_TIMEOUT (-2)  the compiler did not finish within the time limit
 */
constexpr int IFJ26_CRASH = -1;
constexpr int IFJ26_TIMEOUT = -2;
constexpr int IFJ26_PARSER_SEMANTIC = -3;

/** The mandatory prolog (spec chapter 2) that ifj26_exit_code prepends. */
constexpr const char *kIfj26Prolog = "import ifj26.*\n";

/** Seconds a single compilation may take before it counts as a hang. */
constexpr unsigned kIfj26TimeLimitSeconds = 2U;

/** Exit status the child uses to report IFJ26_PARSER_SEMANTIC. */
constexpr int kIfj26ChildParserSemantic = 100;

/** Maps a parser status (other than PARSER_OK) to a chapter 6 exit code. */
inline int ifj26_code_from_parser(ParserStatus status) {
  switch (status) {
  case PARSER_OK:
    return 0;
  case PARSER_LEXER_ERROR:
    return 1;
  case PARSER_SYNTAX_ERROR:
    return 2;
  case PARSER_SEMANTIC_ERROR:
    return IFJ26_PARSER_SEMANTIC;
  default:
    return 99;
  }
}

/** Maps a semantic status to a chapter 6 exit code. */
inline int ifj26_code_from_semantic(SemanticStatus status) {
  return static_cast<int>(status);
}

/** Compiles @p source in this process and returns its exit code. */
inline int ifj26_exit_code_in_process(const char *source) {
  FILE *stream = tmpfile();
  if (stream == nullptr) {
    return 99;
  }
  fputs(source, stream);
  rewind(stream);
  Lexer lexer;
  Program program;
  if (lexer_init(&lexer, stream) != LEXER_OK ||
      program_init(&program) != PARSER_OK) {
    fclose(stream);
    return 99;
  }
  ParserStatus parsed = p_program(&lexer, &program);
  int code = ifj26_code_from_parser(parsed);
  if (parsed == PARSER_OK) {
    code = ifj26_code_from_semantic(semantic_check(&program));
  }
  program_destroy(&program);
  lexer_destroy(&lexer);
  fclose(stream);
  return code;
}

/** Converts a waitpid status of the compiling child into a harness code. */
inline int ifj26_code_from_wait(int wait_status) {
  if (WIFSIGNALED(wait_status)) {
    return WTERMSIG(wait_status) == SIGALRM ? IFJ26_TIMEOUT : IFJ26_CRASH;
  }
  int code = WEXITSTATUS(wait_status);
  return code == kIfj26ChildParserSemantic ? IFJ26_PARSER_SEMANTIC : code;
}

/**
 * Compiles @p source exactly as given (no prolog is added) in a forked child
 * and returns the exit code of spec chapter 6 or a harness-only result.
 */
inline int ifj26_exit_code_raw(const char *source) {
  fflush(stdout);
  fflush(stderr);
  pid_t child = fork();
  if (child < 0) {
    return 99;
  }
  if (child == 0) {
    alarm(kIfj26TimeLimitSeconds);
    int code = ifj26_exit_code_in_process(source);
    exit(code == IFJ26_PARSER_SEMANTIC ? kIfj26ChildParserSemantic : code);
  }
  int wait_status = 0;
  if (waitpid(child, &wait_status, 0) != child) {
    return 99;
  }
  return ifj26_code_from_wait(wait_status);
}

/** True when IFJ26_HARNESS_NO_PROLOG=1 asks to compile programs bare. */
inline bool ifj26_harness_skips_prolog() {
  const char *flag = getenv("IFJ26_HARNESS_NO_PROLOG");
  return flag != nullptr && strcmp(flag, "1") == 0;
}

/**
 * Compiles the spec-correct program body @p source (everything after the
 * prolog) and returns its exit code; the prolog is prepended unless
 * IFJ26_HARNESS_NO_PROLOG=1.
 */
inline int ifj26_exit_code(const char *source) {
  if (ifj26_harness_skips_prolog()) {
    return ifj26_exit_code_raw(source);
  }
  std::string full = std::string(kIfj26Prolog) + source;
  return ifj26_exit_code_raw(full.c_str());
}

#endif // TESTS_SPEC_HARNESS_H_
