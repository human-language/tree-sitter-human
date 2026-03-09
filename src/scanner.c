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
  uint16_t pending_indent;
  bool has_pending;
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
  buffer[n++] = (char)(s->pending_indent & 0xFF);
  buffer[n++] = (char)(s->pending_indent >> 8);
  buffer[n++] = (char)s->has_pending;
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
  s->pending_indent = 0;
  s->has_pending = false;
  if (length < 5) return;
  unsigned p = 0;
  s->depth = (uint8_t)buffer[p++];
  s->queued_dedents = (uint8_t)buffer[p++];
  s->pending_indent = (uint16_t)((unsigned char)buffer[p] |
                                  ((unsigned char)buffer[p + 1] << 8));
  p += 2;
  s->has_pending = (bool)buffer[p++];
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

  /* 2. Emit pending INDENT/DEDENT from a previous NEWLINE scan. */
  if (s->has_pending) {
    uint16_t indent = s->pending_indent;
    uint16_t current = s->stack[s->depth - 1];
    s->has_pending = false;

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

    /* Same indent level -- no token to emit. */
    return false;
  }

  /* 3. At EOF, close open blocks. */
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

  /* 4. Must be sitting on a newline to proceed. */
  if (lexer->lookahead != '\n' && lexer->lookahead != '\r')
    return false;
  if (!valid_symbols[NEWLINE])
    return false;

  /* Consume the newline character(s). */
  if (lexer->lookahead == '\r') {
    lexer->advance(lexer, false);
    if (lexer->lookahead == '\n') lexer->advance(lexer, false);
  } else {
    lexer->advance(lexer, false);
  }

  /*
   * Look ahead to find the indent level of the next non-blank line.
   * We skip blank lines entirely (consume them into this NEWLINE token).
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
     * Found a non-blank line. Record the indent for the NEXT scan call
     * and emit NEWLINE now. The INDENT/DEDENT will be emitted on the
     * next call via the pending mechanism.
     */
    lexer->mark_end(lexer);

    uint16_t current = s->stack[s->depth - 1];
    if (indent != current) {
      s->pending_indent = indent;
      s->has_pending = true;
    }

    lexer->result_symbol = NEWLINE;
    return true;
  }

  /* EOF after skipping blank lines. */
  lexer->mark_end(lexer);
  lexer->result_symbol = NEWLINE;
  return true;
}
