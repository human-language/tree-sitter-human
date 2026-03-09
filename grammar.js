/// <reference types="tree-sitter-cli/dsl" />
// @ts-check

module.exports = grammar({
  name: "hmn",

  externals: $ => [
    $._indent,
    $._dedent,
    $._newline,
  ],

  extras: $ => [/[ \t]/],

  rules: {
    source_file: $ => repeat($._toplevel),

    _toplevel: $ => choice(
      $.import_statement,
      $.agent_declaration,
      $.system_directive,
      $.constraints_block,
      $.flow_block,
      $.test_block,
      $.comment,
    ),

    import_statement: $ => seq(
      "IMPORT",
      choice($.path, $.package),
      $._newline,
    ),

    agent_declaration: $ => seq(
      "AGENT",
      field("name", $.identifier),
      $._newline,
      optional($.agent_body),
    ),

    agent_body: $ => seq(
      $._indent,
      repeat1($._agent_member),
      $._dedent,
    ),

    _agent_member: $ => choice(
      $.system_directive,
      $.property,
      $.comment,
    ),

    system_directive: $ => seq(
      "SYSTEM",
      field("path", $.path),
      $._newline,
    ),

    property: $ => seq(
      field("key", $.identifier),
      "=",
      field("value", $._value),
      $._newline,
    ),

    _value: $ => choice(
      $.string,
      $.number,
      $.boolean,
      $.path,
    ),

    constraints_block: $ => seq(
      "CONSTRAINTS",
      field("name", $.identifier),
      $._newline,
      $._indent,
      repeat1($.constraint),
      $._dedent,
    ),

    constraint: $ => seq(
      $.constraint_level,
      $.constraint_text,
      $._newline,
    ),

    constraint_level: $ => choice(
      "NEVER",
      "MUST",
      "SHOULD",
      "AVOID",
      "MAY",
    ),

    constraint_text: $ => /[^\n\r]+/,

    flow_block: $ => seq(
      "FLOW",
      field("name", $.identifier),
      $._newline,
      $._indent,
      repeat1($.flow_step),
      $._dedent,
    ),

    flow_step: $ => seq(
      $.step_text,
      $._newline,
    ),

    step_text: $ => /[^\n\r]+/,

    test_block: $ => seq(
      "TEST",
      $._newline,
      $._indent,
      repeat1($._test_entry),
      $._dedent,
    ),

    _test_entry: $ => choice(
      $.test_input,
      $.test_expect,
    ),

    test_input: $ => seq(
      "INPUT",
      $.string,
      $._newline,
    ),

    test_expect: $ => seq(
      "EXPECT",
      optional("NOT"),
      choice("CONTAINS", "MATCHES"),
      $.string,
      $._newline,
    ),

    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,

    path: $ => /\.\.?\/[^\n\r \t]*/,

    package: $ => /[a-zA-Z][a-zA-Z0-9._\/-]*/,

    string: $ => seq(
      '"',
      optional($.string_content),
      '"',
    ),

    string_content: $ => /([^"\\]|\\["\\])+/,

    number: $ => /[0-9]+(\.[0-9]+)?/,

    boolean: $ => choice("true", "false"),

    comment: $ => seq(token.immediate(/#/), /[^\n\r]*/),
  },
});
