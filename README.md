supercomplex
============

`supercomplex` is a simple DFA-based lexer generator that allows you to quickly produce a lexer written in any target language. 
It accepts the token description in the form of regular expressions. This syntax for regular expressions is supported

| expression | meaning
|:-----------|:--------
| `[0-9]`,`[ATCG]` | character ranges and character sets
| `[^0-9ATCG]` | complements of character sets/ranges
| <code>expr1&#124;expr2&#124;...</code> | subexpression alternatives
| `(expr)` | subexpression grouping
| `expr*` | `expr` zero or multiple times
| `expr+` | `expr` one or multiple times
| `expr?` | `expr` zero or one time

Note that `.` matches a literal period. Use `[^]` to match any character. The following characters must be escaped (with a `\`): `[`, `]`, `(`, `)`, `+`, `-`, `*`, `\`, `?`. Otherwise, any character encountered is treated as literal (even `'\0'`). `supercomplex` is templated so that it works with any standard C++ char type (`char`, `wchar_t`, `char32_t`, ...), so generating lexer accepting Unicode characters should not be a problem. Note that regular expressions must be of the same character type as the string being matched.

`supercomplex` uses standard Thompson's construction to generate a non-deterministic finite automaton based on the regular expression, then subset construction algorithm to transform said NFA to DFA. It can also minimize the number of states using Moore's algorithm.

Since `supercomplex` is target-agnostic, you have to provide your own code generator for the target language. See `examples/codegen_c.cpp` for an example code generator for C programming language. The example has the lexical description hardcoded - it generates a lexer accepting simple arithmetic expressions, but this can very simply be extended.
