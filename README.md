supercomplex
============

`supercomplex` is a simple C++ library that enables you to quickly write DFA-based lexer generators for any target
language. This is useful for example, when writing a self-hosting compiler for a new language that does not yet have
any standard lexer generators, such as `flex` or ANTLR.

It accepts the token description in the form of regular expressions (proper regular languages without extensions).

It uses Thompson's construction algorithm to generate a non-deterministic finite automaton based on the regular
expression, then subset construction to transform said NFA to DFA. It can also minimize the number of states using
Moore's algorithm.

The following syntax for regular expressions is supported (standard rules of precedence apply):

| expression | meaning
|:-----------|:--------
| `[0-9]`,`[ATCG]` | character ranges and character sets
| `[^0-9ATCG]` | complements of character sets/ranges
| <code>expr1&#124;expr2&#124;...</code> | subexpression alternatives
| `(expr)` | subexpression grouping
| `(expr)*` | `expr` zero or multiple times
| `(expr)+` | `expr` one or multiple times
| `(expr)?` | `expr` zero or one time

Note that `.` matches a literal period. Use `[^]` to match any character. The following characters must be escaped
(with a `\`): `[`, `]`, `(`, `)`, `+`, `-`, `*`, `|`, `\`, `?`. Otherwise, any character encountered is treated as
literal (even `'\0'`). Regex can also be made to match empty string i.e. `foo|bar|` is valid (can be written as
`foo|bar|()` for extra clarity). Empty expressions can be quantified, so `()+` is a valid expression (though useless),
but quantifiers always require something on their left hand side. `+` and `foo|*` are therefore not valid expressions.
Characters can be superfluously escaped - `\4` is a valid expression equivalent to `4`. Note however, that there are no
standard escape sequences common in programming languages - `\n` is equivalent to `n`, not a newline.

`supercomplex` is templated so that it works with any standard C++ char type (`char`, `wchar_t`, `char32_t`, ...), so
generating lexer accepting Unicode characters should not be a problem. Note that regular expressions must be of the same
 character type as the string being matched.

Since `supercomplex` is target-agnostic, you have to provide your own code generator for the target language. See
`examples/codegen_cpp_json.cpp` to see how `supercomplex` can be used to generate a JSON lexer for C++ language target.