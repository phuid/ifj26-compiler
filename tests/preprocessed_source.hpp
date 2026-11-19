/**
 * @file preprocessed_source.hpp
 * @brief Test helper: removes comments the way main.c does before lexing.
 *
 * AI-generated helper.
 * Model: claude-opus-5-5 (Claude Code).
 * Prompt: Comments are removed by source_preprocess before the lexer runs,
 *   so lexer tests with comments must lex the preprocessed source. Write a
 *   helper that preprocesses a prolog-less snippet and keeps line numbers.
 */

#ifndef TESTS_PREPROCESSED_SOURCE_HPP
#define TESTS_PREPROCESSED_SOURCE_HPP

#include <string>

extern "C" {
#include "../src/source_preprocess.h"
}

/** Result of preprocessing one snippet. */
struct PreprocessedSource {
  PreprocessStatus status;
  std::string text; ///< Comment-free source; empty unless status is OK.
};

/**
 * Runs @p snippet through source_preprocess. The snippet has no prolog, so
 * one is added first; preprocessing keeps the prolog's newline, which is
 * dropped again so line numbers match @p snippet.
 */
inline PreprocessedSource preprocess_snippet(const std::string &snippet) {
  std::string buffer = "import ifj26.*\n" + snippet;
  PreprocessStatus status = source_preprocess(&buffer[0]);
  if (status != PREPROCESS_OK) {
    return {status, ""};
  }
  std::string text(buffer.c_str());
  return {status, text.empty() ? text : text.substr(1)};
}

#endif // TESTS_PREPROCESSED_SOURCE_HPP
