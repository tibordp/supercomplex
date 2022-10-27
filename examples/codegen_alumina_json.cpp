#include <iomanip>
#include <iostream>
#include <ostream>
#include <string>

#include "supercomplex.hpp"

using namespace std;
using namespace supercomplex;

struct t_info {
    std::string name;
    bool skip;
};

/* Represent character as an Alumina character literal to make the output a bit more
 * readable */
std::string represent_char(char ch) {
    std::stringstream stream;
    stream << "'";

    if (ch == '\r')
        stream << "\\r";
    else if (ch == '\n')
        stream << "\\n";
    else if (ch == '\t')
        stream << "\\t";
    else if (ch == '\\')
        stream << "\\\\";
    else if (ch == '\'')
        stream << "\'";
    else if ((ch >= ' ') && (ch < '\x7f'))
        stream << ch;
    else {
        stream << "\\x" << std::setfill('0') << std::setw(2) << std::hex
               << (static_cast<int>(ch) & 0xFF);
    }

    stream << "'";
    return stream.str();
}

/* Represent interval set as a bool condition that can be used in IF clause */
void ranges(
    std::basic_ostream<char>& out,
    const std::basic_string<char>& name,
    boost::icl::interval_set<char> range
) {
    if (boost::icl::interval_count(range) > 1)
        out << "(";
    bool first = true;
    for (auto&& interval : range) {
        if (!first) {
            out << "\n                        || ";
        }
        if (interval.lower() == interval.upper())
            out << name << " == " << represent_char(interval.upper());
        else
            out << "(" << name << " >= " << represent_char(interval.lower()) << " && " << name
                << " <= " << represent_char(interval.upper()) << ")";
        first = false;
    }
    if (boost::icl::interval_count(range) > 1)
        out << ")";
}

int alumina_codegen(
    std::basic_ostream<char>& out,
    const supercomplex::lexer<char, t_info>& automaton
) {
    constexpr std::string_view token_type_class = "TokenType";
    constexpr std::string_view token_class = "Token";
    constexpr std::string_view iterator_class = "LexerIterator";
    constexpr std::string_view error_type_class = "LexerErrorType";
    constexpr std::string_view error_class = "LexerError";

    out << "use std::iter::{Iterator, IteratorExt};" << std::endl;
    out << "use std::cmp::Equatable;" << std::endl;
    out << "use std::fmt::{Formattable, Formatter, write};" << std::endl << std::endl;

    out << "enum " << token_type_class << " {" << std::endl;
    std::unordered_set<std::string> visited_terminals;
    std::stringstream fmt_variants;

    /* We find all terminal nodes with names to populate the enum for token types.
     */
    for (auto&& state : automaton.states()) {
        if (state.terminal && !state.terminal_info.skip &&
            visited_terminals.find(state.terminal_info.name) == visited_terminals.end()) {
            out << "    " << state.terminal_info.name << "," << std::endl;
            fmt_variants << "            " << token_type_class << "::" << state.terminal_info.name
                         << " => write!(f, \"" << state.terminal_info.name << "\")," << std::endl;
            visited_terminals.insert(state.terminal_info.name);
        }
    }

    out << "}" << std::endl << std::endl;

    out << "impl " << token_type_class << " {" << std::endl;
    out << "    fn equals(self: &" << token_type_class << ", other: &" << token_type_class
        << ") -> bool {" << std::endl;
    out << "        *self == *other" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    fn fmt<F: Formatter<F>>(self: &" << token_type_class
        << ", f: &mut F) -> std::fmt::Result {" << std::endl;
    out << "        switch *self {" << std::endl;
    out << fmt_variants.str();
    out << "            _ => unreachable!()," << std::endl;
    out << "        }" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    mixin Equatable<" << token_type_class << ">;" << std::endl;
    out << "}" << std::endl << std::endl;

    out << "struct " << token_class << " {" << std::endl;
    out << "    type: " << token_type_class << "," << std::endl;
    out << "    value: &[u8]," << std::endl;
    out << "}" << std::endl << std::endl;

    out << "impl " << token_class << " {" << std::endl;
    out << "    fn equals(self: &" << token_class << ", other: &" << token_class << ") -> bool {"
        << std::endl;
    out << "        self.type == other.type && self.value == other.value" << std::endl;
    out << "    }" << std::endl << std::endl;

    out << "    fn fmt<F: Formatter<F>>(self: &" << token_class
        << ", f: &mut F) -> std::fmt::Result {" << std::endl;
    out << "        write!(f, \"{}({})\", self.type, self.value)" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "	mixin Equatable<" << token_class << ">;" << std::endl;
    out << "}" << std::endl << std::endl;

    out << "enum " << error_type_class << " {" << std::endl;
    out << "    Unexpected," << std::endl;
    out << "    Eof," << std::endl;
    out << "}" << std::endl << std::endl;

    out << "impl " << error_type_class << " {" << std::endl;
    out << "    fn equals(self: &" << error_type_class << ", other: &" << error_type_class
        << ") -> bool {" << std::endl;
    out << "        *self == *other" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    fn fmt<F: Formatter<F>>(self: &" << error_type_class
        << ", f: &mut F) -> std::fmt::Result {" << std::endl;
    out << "        switch *self {" << std::endl;
    out << "            " << error_type_class
        << "::Unexpected => write!(f, \"unexpected character\")," << std::endl;
    out << "            " << error_type_class << "::Eof => write!(f, \"unexpected end of input\"),"
        << std::endl;
    out << "            _ => unreachable!()," << std::endl;
    out << "        }" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    mixin Equatable<" << error_type_class << ">;" << std::endl;
    out << "}" << std::endl << std::endl;

    out << "struct " << error_class << " {" << std::endl;
    out << "    type: " << error_type_class << "," << std::endl;
    out << "    position: usize," << std::endl;
    out << "}" << std::endl << std::endl;

    out << "impl " << error_class << " {" << std::endl;
    out << "    fn equals(self: &" << error_class << ", other: &" << error_class << ") -> bool {"
        << std::endl;
    out << "        self.type == other.type && self.position == other.position" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    fn fmt<F: Formatter<F>>(self: &" << error_class
        << ", f: &mut F) -> std::fmt::Result {" << std::endl;
    out << "        write!(f, \"{} at position {}\", self.type, self.position)" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    mixin Equatable<" << error_class << ">;" << std::endl;
    out << "}" << std::endl << std::endl;

    out << "struct " << iterator_class << " {" << std::endl;
    out << "    state: i32," << std::endl;
    out << "    value: &[u8]," << std::endl;
    out << "    start: usize," << std::endl;
    out << "    end: usize," << std::endl;
    out << "}" << std::endl << std::endl;

    out << "impl " << iterator_class << " {" << std::endl;
    out << "    fn new(value: &[u8]) -> " << iterator_class << " {" << std::endl;
    out << "        " << iterator_class << " {" << std::endl;
    out << "            state: " << automaton.start() << "," << std::endl;
    out << "            value: value," << std::endl;
    out << "            start: 0," << std::endl;
    out << "            end: 0," << std::endl;
    out << "        }" << std::endl;
    out << "    }" << std::endl << std::endl;

    out << "    fn next(self: &mut " << iterator_class << ") -> Option<Result<" << token_class
        << ", " << error_class << ">> {" << std::endl;
    out << "        macro token($type) {" << std::endl;
    out << "            let token_text = self.value[self.start..self.end];" << std::endl;
    out << "            self.start = self.end;" << std::endl;
    out << "            return Option::some(Result::ok(" << token_class << " {" << std::endl;
    out << "                type: $type," << std::endl;
    out << "                value: token_text," << std::endl;
    out << "            }));" << std::endl;
    out << "        }" << std::endl << std::endl;

    out << "        macro has_next() {" << std::endl;
    out << "            self.end < self.value.len()" << std::endl;
    out << "        }" << std::endl << std::endl;

    out << "        macro bail($error_type) {" << std::endl;
    out << "            return Option::some(Result::err(" << error_class << " {" << std::endl;
    out << "                type: $error_type," << std::endl;
    out << "                position: self.end," << std::endl;
    out << "            }));" << std::endl;
    out << "        }" << std::endl << std::endl;

    out << "        macro ch() {" << std::endl;
    out << "            self.value[self.end]" << std::endl;
    out << "        }" << std::endl << std::endl;

    out << "        loop {" << std::endl;
    out << "            switch self.state {" << std::endl;
    const auto& states = automaton.states();

    for (size_t i = 0; i < states.size(); ++i) {
        const auto& state = states[i];
        out << "                " << i << " => {" << std::endl;
        bool first = true;
        for (auto&& transition : state.transitions) {
            if (first) {
                out << "                    if has_next!() &&" << std::endl;
                out << "                        ";
                first = false;
            } else {
                out << "                    else if has_next!() &&" << std::endl;
                out << "                        ";
            }
            ranges(out, "ch!()", transition.characters);
            out << " {" << std::endl;
            if (i != transition.next) {
                out << "                        self.state = " << transition.next << ";"
                    << std::endl;
            }
            out << "                    }" << std::endl;
        }

        if (state.terminal) {
            if (!first) {
                out << "                    else {" << std::endl;
            }
            std::string_view indent = first ? "" : "    ";
            auto terminal_node = state.terminal_info;
            out << indent << "                    self.state = " << automaton.start() << ";"
                << std::endl;
            if (terminal_node.skip) {
                out << indent << "                    self.start = self.end;" << std::endl;
                out << indent << "                    continue;" << std::endl;
            } else {
                out << indent << "                    token!(" << token_type_class
                    << "::" << terminal_node.name << ");" << std::endl;
            }
            if (!first) {
                out << "                    }" << std::endl;
            }
        } else {
            if (!first) {
                out << "                    else if !has_next!() {" << std::endl;
            } else {
                out << "                    if !has_next!() {" << std::endl;
            }

            if (i == automaton.start()) {
                out << "                        return Option::none();" << std::endl;
            } else {
                out << "                        bail!(" << error_type_class << "::Eof);"
                    << std::endl;
            }
            out << "                    } else {" << std::endl;
            out << "                        bail!(" << error_type_class << "::Unexpected);"
                << std::endl;
            out << "                    }" << std::endl;
        }
        out << "                }," << std::endl;
    }
    out << "                _ => unreachable!()," << std::endl;
    out << "            }" << std::endl << std::endl;
    out << "            self.end += 1;" << std::endl;
    out << "        }" << std::endl;
    out << "    }" << std::endl << std::endl;
    out << "    mixin Iterator<" << iterator_class << ", " << token_class << ">;" << std::endl;
    out << "    mixin IteratorExt<" << iterator_class << ", " << token_class << ">;" << std::endl;
    out << "}" << std::endl << std::endl;

    out << "#[cfg(test)]" << std::endl;
    out << "mod tests {" << std::endl;
    out << "    #[test]" << std::endl;
    out << "    fn test_basic() {" << std::endl;
    out << "        let it = " << iterator_class << "::new(\"\");" << std::endl << std::endl;
    out << "        assert!(it.next().is_none());" << std::endl;
    out << "    }" << std::endl;
    out << "}" << std::endl;

    return 0;
}

lexer_production<char, t_info> prod(const std::string& name, const std::string& regex) {
    return { { name, false }, regex };
}

lexer_production<char, t_info> skip(const std::string& regex) {
    return { { std::string(), true }, regex };
}

int main() {
    /*
      This will generate Alumina code for a JSON lexer.
    */
    lexer_generator<char, t_info> lex_gen;
    lex_gen
        << prod("ArrOpen", "\\[") << prod("ArrClose", "\\]") << prod("ObjOpen", "{")
        << prod("ObjClose", "}") << prod("Literal", "true|false|null") << prod("Comma", ",")
        << prod("Colon", ":")
        << prod(
               "String",
               "\"(\\\\([\"\\\\/bfrnt]|u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])|[^\"\\\\\0-\x1f])*\""s
           )
        << prod("Number", "-?(0|[1-9][0-9]*)(.[0-9]+)?([Ee][+\\-]?(0|[1-9][0-9]*))?")
        << skip("[ \t\n\r]+");

    auto lexer = lex_gen.generate();
    alumina_codegen(std::cout, lexer);
}
