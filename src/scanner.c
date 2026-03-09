#include "tree_sitter/parser.h"
#include <stdlib.h>

enum TokenType {
  INDENT,
  DEDENT,
  NEWLINE,
};

#define MAX_DEPTH 32

typedef struct {
  uint16_t stack[MAX_DEPTH];
  uint8_t depth;
  uint8_t queued_dedents;
} Scanner;

void *tree_sitter_hmn_external_scanner_create(void) {
  Scanner *s = calloc(1, sizeof(Scanner));
  s->stack[0] = 0;
  s->depth = 1;
  return s;
}

void tree_sitter_hmn_external_scanner_destroy(void *payload) {
  free(payload);
}

unsigned tree_sitter_hmn_external_scanner_serialize(void *payload, char *buffer) {
  Scanner *s = payload;
  unsigned n = 0;
  buffer[n++] = (char)s->depth;
  buffer[n++] = (char)s->queued_dedents;
  for (uint8_t i = 0; i < s->depth && n + 1 < TREE_SITTER_SERIALIZATION_BUFFER_SIZE; i++) {
    buffer[n++] = (char)(s->stack[i] & 0xFF);
    buffer[n++] = (char)(s->stack[i] >> 8);
  }
  return n;
}

void tree_sitter_hmn_external_scanner_deserialize(
  void *payload, const char *buffer, unsigned length
) {
  Scanner *s = payload;
  s->stack[0] = 0;
  s->depth = 1;
  s->queued_dedents = 0;
  if (length < 2) return;
  unsigned p = 0;
  s->depth = (uint8_t)buffer[p++];
  s->queued_dedents = (uint8_t)buffer[p++];
  if (s->depth > MAX_DEPTH) s->depth = MAX_DEPTH;
  for (uint8_t i = 0; i < s->depth && p + 1 < length; i++) {
    s->stack[i] = (uint16_t)((unsigned char)buffer[p] |
                              ((unsigned char)buffer[p + 1] << 8));
    p += 2;
  }
}

bool tree_sitter_hmn_external_scanner_scan(
  void *payload,
  TSLexer *lexer,
  const bool *valid_symbols
) {
  Scanner *s = payload;

  /* 1. Drain queued dedents (zero-width). */
  if (s->queued_dedents > 0 && valid_symbols[DEDENT]) {
    s->queued_dedents--;
    lexer->result_symbol = DEDENT;
    return true;
  }

  /* 2. At EOF, close open blocks. */
  if (lexer->eof(lexer)) {
    if (valid_symbols[DEDENT] && s->depth > 1) {
      s->depth--;
      lexer->result_symbol = DEDENT;
      return true;
    }
    if (valid_symbols[NEWLINE]) {
      lexer->result_symbol = NEWLINE;
      return true;
    }
    return false;
  }

  /* 3. Must be sitting on a newline to proceed. */
  if (lexer->lookahead != '\n' && lexer->lookahead != '\r')
    return false;
  if (!valid_symbols[NEWLINE] && !valid_symbols[INDENT] && !valid_symbols[DEDENT])
    return false;

  /*
   * Consume the newline. We use advance(false) so the token has
   * nonzero width and tree-sitter knows progress was made.
   */
  if (lexer->lookahead == '\r') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') lexer->advance(lexer, false);
  } else {
    lexer->advance(lexer, false);
  }

  /*
   * Now look ahead to find the indent level of the next non-blank line.
   * We DON'T call mark_end yet -- if we decide to emit INDENT or DEDENT
   * instead of NEWLINE, the token still starts from the \n we consumed.
   * After deciding, mark_end will be placed right after the \n (before
   * any spaces we peeked past), unless we consumed more blank lines.
   */

  int guard = 0;
  for (;;) {
    if (lexer->eof(lexer) || ++guard > 5000) break;

    /* Count leading spaces. */
    uint16_t indent = 0;
    while (lexer->lookahead == ' ') {
      indent++;
      lexer->advance(lexer, false);
    }
    /* Skip tabs (shouldn't be used but be safe). */
    while (lexer->lookahead == '\t') {
      lexer->advance(lexer, false);
    }

    if (lexer->eof(lexer)) break;

    /* Blank line -- consume newline and keep going. */
    if (lexer->lookahead == '\n' || lexer->lookahead == '\r') {
      if (lexer->lookahead == '\r') {
        lexer->advance(lexer, false);
        if (lexer->lookahead == '\n') lexer->advance(lexer, false);
      } else {
        lexer->advance(lexer, false);
      }
      continue;
    }

    /*
     * Found a non-blank line. `indent` is the number of leading spaces.
     * Mark end here -- the token encompasses everything from the
     * original \n through any blank lines and the leading spaces.
     */
    lexer->mark_end(lexer);

    uint16_t current = s->stack[s->depth - 1];

    if (indent > current && valid_symbols[INDENT]) {
      if (s->depth < MAX_DEPTH) {
        s->stack[s->depth] = indent;
        s->depth++;
      }
      lexer->result_symbol = INDENT;
      return true;
    }

    if (indent < current && valid_symbols[DEDENT]) {
      uint8_t pops = 0;
      while (s->depth > 1 && s->stack[s->depth - 1] > indent) {
        s->depth--;
        pops++;
      }
      if (pops > 1) {
        s->queued_dedents = pops - 1;
      }
      lexer->result_symbol = DEDENT;
      return true;
    }

    if (valid_symbols[NEWLINE]) {
      lexer->result_symbol = NEWLINE;
      return true;
    }

    return false;
  }

  /* EOF after skipping blank lines. */
  lexer->mark_end(lexer);
  if (valid_symbols[DEDENT] && s->depth > 1) {
    s->depth--;
    lexer->result_symbol = DEDENT;
    return true;
  }
  if (valid_symbols[NEWLINE]) {
    lexer->result_symbol = NEWLINE;
    return true;
  }
  return false;
}
