#include <cstdio>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "gtest/gtest.h"

extern "C" {
#include "../src/lexer.h"
#include "../src/lexer_dfa.h"
}

namespace {
/*
 * AI-generated test block — independent KA.md transcription for the DFA.
 * Model: Claude (Claude Code).
 * Prompt:
 *   Transcribe the KA.md transitions (not lexer_dfa.c) as {from, predicate,
 *   to} edges with the KA character sets DIGIT, LETTER, C and M, plus
 *   readable state and input names for failure messages.
 */
using Pred = std::function<bool(int)>;

Pred ch(int expected) {
  return [expected](int c) { return c == expected; };
}
Pred any(std::initializer_list<int> chars) {
  std::vector<int> set(chars);
  return [set](int c) {
    for (int member : set) {
      if (c == member) {
        return true;
      }
    }
    return false;
  };
}
Pred rng(int low, int high) {
  return [low, high](int c) { return c >= low && c <= high; };
}
const Pred digit = rng('0', '9');
const Pred nz = rng('1', '9');
const Pred letter = [](int c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
};
// KA.md: C = { c in ASCII | c != LF, c != '"', c != '\' }.
const Pred C = [](int c) {
  return c >= 0 && c <= 127 && c != '\n' && c != '"' && c != '\\';
};
// KA.md: M = { c in ASCII | c != '"' }.
const Pred M = [](int c) { return c >= 0 && c <= 127 && c != '"'; };
const Pred idc = [](int c) { return letter(c) || digit(c) || c == '_'; };

struct KaEdge {
  LexerState from;
  Pred pred;
  LexerState to;
};

std::vector<KaEdge> ka_edges() {
  return {
      // Numbers (Int, Double)
      {STATE_S, ch('0'), STATE_INT0},
      {STATE_S, nz, STATE_INT},
      {STATE_S, ch('.'), STATE_PERIOD},
      {STATE_INT0, ch('.'), STATE_NUM_DOT},
      {STATE_INT0, any({'e', 'E'}), STATE_EXP},
      {STATE_INT0, digit, STATE_DOUBLE_STARTS_ZERO},
      {STATE_INT0, ch('_'), STATE_U_DOUBLE_STARTS_ZERO},
      {STATE_DOUBLE_STARTS_ZERO, digit, STATE_DOUBLE_STARTS_ZERO},
      {STATE_DOUBLE_STARTS_ZERO, ch('_'), STATE_U_DOUBLE_STARTS_ZERO},
      {STATE_U_DOUBLE_STARTS_ZERO, ch('_'), STATE_U_DOUBLE_STARTS_ZERO},
      {STATE_U_DOUBLE_STARTS_ZERO, digit, STATE_DOUBLE_STARTS_ZERO},
      {STATE_DOUBLE_STARTS_ZERO, ch('.'), STATE_NUM_DOT},
      {STATE_DOUBLE_STARTS_ZERO, any({'e', 'E'}), STATE_EXP},
      {STATE_INT, digit, STATE_INT},
      {STATE_INT, ch('_'), STATE_U_INT},
      {STATE_U_INT, ch('_'), STATE_U_INT},
      {STATE_U_INT, digit, STATE_INT},
      {STATE_INT, ch('.'), STATE_NUM_DOT},
      {STATE_INT, any({'e', 'E'}), STATE_EXP},
      {STATE_NUM_DOT, digit, STATE_DOUBLE},
      {STATE_PERIOD, digit, STATE_DOUBLE},
      {STATE_DOUBLE, digit, STATE_DOUBLE},
      {STATE_DOUBLE, ch('_'), STATE_U_DOUBLE},
      {STATE_U_DOUBLE, ch('_'), STATE_U_DOUBLE},
      {STATE_U_DOUBLE, digit, STATE_DOUBLE},
      {STATE_DOUBLE, any({'e', 'E'}), STATE_EXP},
      {STATE_EXP, digit, STATE_E_DOUBLE},
      {STATE_EXP, any({'+', '-'}), STATE_S_EXP},
      {STATE_S_EXP, digit, STATE_E_DOUBLE},
      {STATE_E_DOUBLE, ch('_'), STATE_E_U_DOUBLE},
      {STATE_E_U_DOUBLE, ch('_'), STATE_E_U_DOUBLE},
      {STATE_E_U_DOUBLE, digit, STATE_E_DOUBLE},
      {STATE_E_DOUBLE, digit, STATE_E_DOUBLE},
      {STATE_DOUBLE, any({'d', 'D'}), STATE_DOUBLE_FINAL},
      {STATE_E_DOUBLE, any({'d', 'D'}), STATE_DOUBLE_FINAL},
      // Strings
      {STATE_S, ch('"'), STATE_Q1},
      {STATE_Q1, C, STATE_IN},
      {STATE_Q1, ch('\\'), STATE_ESC},
      {STATE_Q1, ch('"'), STATE_STR_EMPTY},
      {STATE_STR_EMPTY, ch('"'), STATE_IN_MULTILINE},
      {STATE_IN, C, STATE_IN},
      {STATE_IN, ch('\\'), STATE_ESC},
      {STATE_IN, ch('"'), STATE_STR_NORMAL},
      {STATE_ESC, any({'\\', '"', 'n', 'r', 't'}), STATE_IN},
      {STATE_IN_MULTILINE, M, STATE_IN_MULTILINE},
      {STATE_IN_MULTILINE, ch('"'), STATE_Q1_MULTI},
      {STATE_Q1_MULTI, ch('"'), STATE_Q2_MULTI},
      {STATE_Q2_MULTI, ch('"'), STATE_STR_MULTILINE},
      {STATE_STR_MULTILINE, ch('"'), STATE_STR_MULTILINE},
      {STATE_Q1_MULTI, M, STATE_IN_MULTILINE},
      {STATE_Q2_MULTI, M, STATE_IN_MULTILINE},
      // Identifiers
      {STATE_S, letter, STATE_IDENTIFIER},
      {STATE_S, ch('_'), STATE_UNDERSCORE},
      {STATE_UNDERSCORE, idc, STATE_IDENTIFIER},
      {STATE_IDENTIFIER, idc, STATE_IDENTIFIER},
      // Others
      {STATE_S, ch('='), STATE_ASSIGN},
      {STATE_ASSIGN, ch('='), STATE_EQUAL},
      {STATE_S, ch('!'), STATE_NOT},
      {STATE_S, ch('&'), STATE_AND_PREFIX},
      {STATE_AND_PREFIX, ch('&'), STATE_AND},
      {STATE_NOT, ch('='), STATE_NOT_EQUAL},
      {STATE_ASSIGN, ch('>'), STATE_ARROW},
      {STATE_S, ch('>'), STATE_GREATER},
      {STATE_S, ch('<'), STATE_LESS},
      {STATE_GREATER, ch('='), STATE_GREATER_EQUAL},
      {STATE_LESS, ch('='), STATE_LESS_EQUAL},
      {STATE_LESS, ch('-'), STATE_GENERATOR},
      {STATE_S, ch('@'), STATE_AT},
      {STATE_AT, ch('m'), STATE_AT_M},
      {STATE_AT_M, ch('a'), STATE_AT_MA},
      {STATE_AT_MA, ch('i'), STATE_AT_MAI},
      {STATE_AT_MAI, ch('n'), STATE_AT_MAIN},
      {STATE_S, ch('+'), STATE_PLUS},
      {STATE_PLUS, ch('+'), STATE_CONCAT},
      {STATE_S, ch('-'), STATE_MINUS},
      {STATE_S, ch('*'), STATE_MULTIPLY},
      {STATE_S, ch('/'), STATE_DIVIDE},
      {STATE_S, ch('('), STATE_LEFT_PAREN},
      {STATE_S, ch(')'), STATE_RIGHT_PAREN},
      {STATE_S, ch(':'), STATE_COLON},
      {STATE_S, ch('['), STATE_LEFT_BRACKET},
      {STATE_S, ch(']'), STATE_RIGHT_BRACKET},
      {STATE_S, ch(';'), STATE_SEMICOLON},
      {STATE_S, ch('\n'), STATE_NEWLINE},
      {STATE_NEWLINE, ch(' '), STATE_NEWLINE_SPACES},
      {STATE_NEWLINE, ch('\t'), STATE_NEWLINE_TABS},
      {STATE_NEWLINE, ch('\n'), STATE_NEWLINE},
      {STATE_NEWLINE_SPACES, ch(' '), STATE_NEWLINE_SPACES},
      {STATE_NEWLINE_SPACES, ch('\t'), STATE_NEWLINE_TABS},
      {STATE_NEWLINE_SPACES, ch('\n'), STATE_NEWLINE},
      {STATE_NEWLINE_TABS, any({' ', '\t'}), STATE_NEWLINE_TABS},
      {STATE_NEWLINE_TABS, ch('\n'), STATE_NEWLINE},
      {STATE_S, ch(','), STATE_COMMA},
      {STATE_S, ch('|'), STATE_UNION},
      {STATE_UNION, ch('|'), STATE_OR},
  };
}

struct StateName {
  LexerState state;
  const char *name;
};
const StateName kStateNames[] = {
    {STATE_S, "S"},
    {STATE_INT0, "INT0"},
    {STATE_DOUBLE_STARTS_ZERO, "DOUBLE_STARTS_ZERO"},
    {STATE_U_DOUBLE_STARTS_ZERO, "U_DOUBLE_STARTS_ZERO"},
    {STATE_INT, "INT"},
    {STATE_U_INT, "U_INT"},
    {STATE_NUM_DOT, "NUM_DOT"},
    {STATE_PERIOD, "PERIOD"},
    {STATE_DOUBLE, "DOUBLE"},
    {STATE_U_DOUBLE, "U_DOUBLE"},
    {STATE_EXP, "EXP"},
    {STATE_S_EXP, "S_EXP"},
    {STATE_E_DOUBLE, "E_DOUBLE"},
    {STATE_E_U_DOUBLE, "E_U_DOUBLE"},
    {STATE_DOUBLE_FINAL, "DOUBLE_FINAL"},
    {STATE_Q1, "Q1"},
    {STATE_STR_EMPTY, "STR_EMPTY"},
    {STATE_IN, "IN"},
    {STATE_ESC, "ESC"},
    {STATE_STR_NORMAL, "STR_NORMAL"},
    {STATE_IN_MULTILINE, "IN_MULTILINE"},
    {STATE_Q1_MULTI, "Q1_MULTI"},
    {STATE_Q2_MULTI, "Q2_MULTI"},
    {STATE_STR_MULTILINE, "STR_MULTILINE"},
    {STATE_UNDERSCORE, "UNDERSCORE"},
    {STATE_IDENTIFIER, "IDENTIFIER"},
    {STATE_ASSIGN, "ASSIGN"},
    {STATE_EQUAL, "EQUAL"},
    {STATE_AND_PREFIX, "AND_PREFIX"},
    {STATE_AND, "AND"},
    {STATE_NOT, "NOT"},
    {STATE_NOT_EQUAL, "NOT_EQUAL"},
    {STATE_ARROW, "ARROW"},
    {STATE_GREATER, "GREATER"},
    {STATE_LESS, "LESS"},
    {STATE_GREATER_EQUAL, "GREATER_EQUAL"},
    {STATE_LESS_EQUAL, "LESS_EQUAL"},
    {STATE_GENERATOR, "GENERATOR"},
    {STATE_AT, "AT"},
    {STATE_AT_M, "AT_M"},
    {STATE_AT_MA, "AT_MA"},
    {STATE_AT_MAI, "AT_MAI"},
    {STATE_AT_MAIN, "AT_MAIN"},
    {STATE_PLUS, "PLUS"},
    {STATE_CONCAT, "CONCAT"},
    {STATE_MINUS, "MINUS"},
    {STATE_MULTIPLY, "MULTIPLY"},
    {STATE_DIVIDE, "DIVIDE"},
    {STATE_LEFT_PAREN, "LEFT_PAREN"},
    {STATE_RIGHT_PAREN, "RIGHT_PAREN"},
    {STATE_COLON, "COLON"},
    {STATE_LEFT_BRACKET, "LEFT_BRACKET"},
    {STATE_RIGHT_BRACKET, "RIGHT_BRACKET"},
    {STATE_SEMICOLON, "SEMICOLON"},
    {STATE_NEWLINE, "NEWLINE"},
    {STATE_NEWLINE_SPACES, "NEWLINE_SPACES"},
    {STATE_NEWLINE_TABS, "NEWLINE_TABS"},
    {STATE_COMMA, "COMMA"},
    {STATE_UNION, "UNION"},
    {STATE_OR, "OR"},
};

std::string state_name(int state) {
  for (const StateName &entry : kStateNames) {
    if (entry.state == state) {
      return std::string("STATE_") + entry.name;
    }
  }
  return "state #" + std::to_string(state);
}
std::string input_name(int c) {
  if (c == EOF) {
    return "EOF";
  }
  std::string shown = std::to_string(c);
  if (c == '\n') {
    shown += " '\\n'";
  } else if (c == '\t') {
    shown += " '\\t'";
  } else if (c >= 32 && c < 127) {
    shown += std::string(" '") + static_cast<char>(c) + "'";
  }
  return shown;
}
const int kMaxReported = 10;

/*
 * AI-generated test block — every DFA state and input against KA.md.
 * Model: Claude (Claude Code).
 * Prompt:
 *   For every state and every input in {EOF, 0..255}, lexer_dfa_next_state
 *   must succeed iff exactly one KA.md edge matches, with that edge's target;
 *   the oracle itself must be deterministic. Report at most 10 mismatches.
 */
TEST(LexerDfaTest, TransitionsMatchKaForEveryStateAndInput) {
  const std::vector<KaEdge> edges = ka_edges();
  int mismatches = 0;
  int ambiguous = 0;
  for (int state = 0; state < STATE_COUNT; state += 1) {
    for (int c = EOF; c <= 255; c += 1) {
      int hits = 0;
      LexerState want = STATE_S;
      for (const KaEdge &edge : edges) {
        if (edge.from == state && edge.pred(c)) {
          hits += 1;
          want = edge.to;
        }
      }
      if (hits > 1) {
        ambiguous += 1;
        ADD_FAILURE() << "oracle has " << hits << " edges from "
                      << state_name(state) << " on " << input_name(c);
        continue;
      }
      LexerState got = STATE_COUNT;
      const bool found =
          lexer_dfa_next_state(static_cast<LexerState>(state), c, &got);
      if (found == (hits == 1) && (!found || got == want)) {
        continue;
      }
      mismatches += 1;
      if (mismatches <= kMaxReported) {
        ADD_FAILURE() << state_name(state) << " on " << input_name(c)
                      << ": KA.md "
                      << (hits == 1 ? "-> " + state_name(want)
                                    : std::string("has no edge"))
                      << ", DFA "
                      << (found ? "-> " + state_name(got)
                                : std::string("has no edge"));
      }
    }
  }
  EXPECT_EQ(ambiguous, 0);
  EXPECT_EQ(mismatches, 0) << "at most the first " << kMaxReported
                           << " mismatches are listed above";
}

/*
 * AI-generated test block — KA.md final states and their token types.
 * Model: Claude (Claude Code).
 * Prompt:
 *   lexer_dfa_final_token must accept exactly the 39 KA.md final states with
 *   the documented token types and reject the 19 non-final states.
 */
struct FinalState {
  LexerState state;
  TokenType type;
};

TEST(LexerDfaTest, FinalStatesAndTokenTypesMatchKa) {
  const std::vector<FinalState> finals = {
      {STATE_INT0, TOKEN_INT},
      {STATE_INT, TOKEN_INT},
      {STATE_DOUBLE, TOKEN_DOUBLE},
      {STATE_E_DOUBLE, TOKEN_DOUBLE},
      {STATE_DOUBLE_FINAL, TOKEN_DOUBLE},
      {STATE_STR_EMPTY, TOKEN_STRING},
      {STATE_STR_NORMAL, TOKEN_STRING},
      {STATE_STR_MULTILINE, TOKEN_STRING},
      {STATE_IDENTIFIER, TOKEN_IDENTIFIER},
      {STATE_UNDERSCORE, TOKEN_WILDCARD},
      {STATE_ASSIGN, TOKEN_ASSIGN},
      {STATE_EQUAL, TOKEN_EQUAL},
      {STATE_NOT, TOKEN_NOT},
      {STATE_AND, TOKEN_AND},
      {STATE_NOT_EQUAL, TOKEN_NOT_EQUAL},
      {STATE_ARROW, TOKEN_ARROW},
      {STATE_GREATER, TOKEN_GREATER},
      {STATE_LESS, TOKEN_LESS},
      {STATE_GREATER_EQUAL, TOKEN_GREATER_EQUAL},
      {STATE_LESS_EQUAL, TOKEN_LESS_EQUAL},
      {STATE_GENERATOR, TOKEN_FOR_GENERATOR}, // KA.md: TOKEN_GENERATOR
      {STATE_AT_MAIN, TOKEN_MAIN_ANNOTATION},
      {STATE_PLUS, TOKEN_PLUS},
      {STATE_CONCAT, TOKEN_LIST_CONCAT}, // KA.md: TOKEN_CONCAT
      {STATE_MINUS, TOKEN_MINUS},
      {STATE_MULTIPLY, TOKEN_MULTIPLY},
      {STATE_DIVIDE, TOKEN_DIVIDE},
      {STATE_LEFT_PAREN, TOKEN_LEFT_PAREN},
      {STATE_RIGHT_PAREN, TOKEN_RIGHT_PAREN},
      {STATE_COLON, TOKEN_COLON},
      {STATE_LEFT_BRACKET, TOKEN_LEFT_BRACKET},
      {STATE_RIGHT_BRACKET, TOKEN_RIGHT_BRACKET},
      {STATE_SEMICOLON, TOKEN_SEMICOLON},
      {STATE_NEWLINE, TOKEN_NEWLINE},
      {STATE_NEWLINE_SPACES, TOKEN_NEWLINE},
      {STATE_PERIOD, TOKEN_DOT},
      {STATE_COMMA, TOKEN_COMMA},
      {STATE_UNION, TOKEN_UNION},
      {STATE_OR, TOKEN_OR},
  };
  const std::vector<LexerState> non_finals = {
      STATE_S,
      STATE_U_INT,
      STATE_NUM_DOT,
      STATE_U_DOUBLE,
      STATE_EXP,
      STATE_S_EXP,
      STATE_E_U_DOUBLE,
      STATE_Q1,
      STATE_IN,
      STATE_ESC,
      STATE_IN_MULTILINE,
      STATE_Q1_MULTI,
      STATE_Q2_MULTI,
      STATE_AND_PREFIX,
      STATE_AT,
      STATE_AT_M,
      STATE_AT_MA,
      STATE_AT_MAI,
      STATE_NEWLINE_TABS,
      STATE_DOUBLE_STARTS_ZERO,
      STATE_U_DOUBLE_STARTS_ZERO,
  };
  ASSERT_EQ(finals.size(), 39U);
  ASSERT_EQ(non_finals.size(), 21U);
  // The two lists must partition the whole state set.
  ASSERT_EQ(finals.size() + non_finals.size(),
            static_cast<size_t>(STATE_COUNT));

  int accepted = 0;
  for (int state = 0; state < STATE_COUNT; state += 1) {
    SCOPED_TRACE(state_name(state));
    const FinalState *want = nullptr;
    int listed = 0;
    for (const FinalState &entry : finals) {
      if (entry.state == state) {
        want = &entry;
        listed += 1;
      }
    }
    for (LexerState entry : non_finals) {
      if (entry == state) {
        listed += 1;
      }
    }
    EXPECT_EQ(listed, 1) << "state must be listed exactly once";

    TokenType got = TOKEN_INVALID;
    const bool is_final =
        lexer_dfa_final_token(static_cast<LexerState>(state), &got);
    EXPECT_EQ(is_final, want != nullptr);
    if (is_final) {
      accepted += 1;
    }
    if (is_final && want != nullptr) {
      EXPECT_EQ(got, want->type);
    }
  }
  EXPECT_EQ(accepted, 39);
}
} // namespace
