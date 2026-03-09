; Keywords
[
  "AGENT"
  "CONSTRAINTS"
  "FLOW"
  "TEST"
  "SYSTEM"
  "IMPORT"
  "INPUT"
  "EXPECT"
  "NOT"
  "CONTAINS"
  "MATCHES"
] @keyword

; Constraint levels
(constraint_level) @keyword.modifier

; Block names
(agent_declaration name: (identifier) @type)
(constraints_block name: (identifier) @type)
(flow_block name: (identifier) @type)

; Properties
(property key: (identifier) @property)

; Operators
"=" @operator

; Literals
(string) @string
(string_content) @string
(number) @number
(boolean) @constant.builtin
(path) @string.special.path
(package) @module

; Prose text
(constraint_text) @string.special
(step_text) @string.special

; Comments
(comment) @comment
