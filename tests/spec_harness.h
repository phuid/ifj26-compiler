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

#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/semantic.h"
}

/*
 * Status -> exit code mapping (spec chapter 6), checked against the headers in
 * src/:
 *
 *   ParserStatus (parser.h)        exit code
 *   PARSER_OK                      -> run semantic_check, see below
 *   PARSER_LEXER_ERROR             -> 1   (every LexerStatus error is folded
 *                                          into this one by the parser)
 *   PARSER_SYNTAX_ERROR            -> 2
 *   PARSER_UNDEFINED_ERROR         -> 3
 *   PARSER_REDEFINITION_ERROR      -> 4
 *   PARSER_BAD_CALL_ERROR          -> 5
 *   PARSER_TYPE_ERROR              -> 6
 *   (IFJ26_PARSER_SEMANTIC (-3) is kept only for the old single
 *    PARSER_SEMANTIC_ERROR status; the parser no longer produces it)
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

/*
 * AI-generated test block — legend of root-cause IDs used by spec_*_test.cc.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt:
 *   List every RC-* identifier that the "Fails today:" comments of the spec
 *   conformance tests refer to, one line each: where the cause lives and what
 *   is wrong compared with the specification.
 *
 *   RC-PROLOG       parser.c p_program: `import ifj26.*` is not parsed, every
 *                   program with the prolog ends with 2 (spec 2).
 *   RC-NOPROLOG-OK  parser.c p_program: a missing prolog is accepted and an
 *                   empty source ends with 3 instead of 2 (spec 2).
 *   RC-EXPR         parser.c p_expression is a stub: it consumes nothing and
 *                   returns no node, so val/var gives 99 and an if condition
 *                   or return value gives 2 (spec 2.2, 2.4, 3).
 *   RC-CRASH,       parser.c p_statement/p_code_block: a statement starting
 *   RC-STMT-CRASH   with a term (call, literal, identifier) dereferences NULL
 *                   (SIGSEGV, harness -1) (spec 2.1).
 *   RC-ASSIGN       parser.c p_statement: no production for `id = expr`
 *                   (spec 2.2).
 *   RC-WHILE        parser.c/ast.h/semantic.c: no while production, no loop
 *                   node and no Unit type for it (spec 2.5, 2.3 rule 2).
 *   RC-ONELINE      parser.c p_function: a one-line body is not parsed
 *                   (spec 2.3).
 *   RC-EMPTYBODY    parser.c p_function: `=` followed by EOF leaves a NULL
 *                   body and ends with 99 instead of 2 (spec 2.1, ch. 6).
 *   RC-PSTATUS      parser.h ParserStatus: one PARSER_SEMANTIC_ERROR for
 *                   codes 3/4/5/6, the harness reports -3 (ch. 6).
 *   RC-SEMI         parser.c: `;` between statements is rejected (spec 1.2).
 *   RC-TOPNL        parser.c p_program: blank lines at the top level are not
 *                   skipped (spec 1.2, 2).
 *   RC-PSA          psa.c: get_precedence is a stub and there is no PSA
 *                   driver (spec 3).
 *   RC-UNARY        psa.h/ast.h/semantic.c: unary minus has no terminal, AST
 *                   node or typing rule (spec 3).
 *   RC-PARENNL      psa/parser: NEWLINE/INDENT/DEDENT inside parentheses of
 *                   an expression are not skipped (spec 1.2).
 *   RC-ANNOT-USE    parser.c/semantic.c: an annotated variable is usable in
 *                   or before its own definition, no error 3 (spec 2.2).
 *   RC-COMMENT      lexer: line comments and nested block comments are not
 *                   recognised (spec 1.1).
 *   RC-LEADWS       lexer.c: white space at the start of the file is a
 *                   lexical error (spec 2).
 *   RC-WSEOF        lexer.c update_indent_delta: a last line of spaces emits
 *                   INDENT (spec 1.2).
 *   RC-LEADZERO     lexer_dfa.c STATE_INT0: `007` and `00.5` split into
 *                   several tokens instead of one error/one Double (spec 1.1).
 *   RC-DBLRANGE     lexer.c finish_token: strtod without range check accepts
 *                   Double literals outside binary64 (spec 1.1).
 *   RC-EXT-*        extensions (spec 5): RC-EXT-BOOLTHEN, RC-EXT-CONTROL,
 *                   RC-EXT-VALMEM-LEX, RC-EXT-SYNTAX name the missing lexer,
 *                   parser and semantic support of the extension.
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
#pragma GCC diagnostic push
#pragma GCC diagnostic error "-Wswitch-enum"
inline int ifj26_code_from_parser(ParserStatus status) {
  switch (status) {
  case PARSER_OK:
    return 0;
  case PARSER_LEXER_ERROR:
    return 1;
  case PARSER_SYNTAX_ERROR:
    return 2;
  case PARSER_UNDEFINED_ERROR:
    return 3;
  case PARSER_REDEFINITION_ERROR:
    return 4;
  case PARSER_BAD_CALL_ERROR:
    return 5;
  case PARSER_TYPE_ERROR:
    return 6;
  case PARSER_INVALID_ARGUMENT:
  case PARSER_INTERNAL_ERROR:
    return 99;
  default:
    return 99;
  }
}
#pragma GCC diagnostic pop

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
 * Resets what the child inherits from the test process: default actions and
 * no blocking for the signals the harness reports, and no core dumps.
 */
inline void ifj26_prepare_child() {
  struct rlimit no_core = {0, 0};
  setrlimit(RLIMIT_CORE, &no_core);
  const int reported[] = {SIGALRM, SIGSEGV, SIGABRT, SIGBUS, SIGFPE};
  sigset_t unblocked;
  sigemptyset(&unblocked);
  for (int sig : reported) {
    signal(sig, SIG_DFL);
    sigaddset(&unblocked, sig);
  }
  sigprocmask(SIG_UNBLOCK, &unblocked, nullptr);
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
    ifj26_prepare_child();
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
