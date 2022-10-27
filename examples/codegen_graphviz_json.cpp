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

std::string represent_char(char ch) {
    std::stringstream stream;

    if (ch == '\r')
        stream << "\\\\r";
    else if (ch == '\n')
        stream << "\\\\n";
    else if (ch == '\t')
        stream << "\\\\t";
    else if (ch == '\\')
        stream << "\\\\";
    else if (ch == '\'')
        stream << "\\\'";
    else if (ch == '\"')
        stream << "\\\"";
    else if ((ch >= ' ') && (ch < '\x7f'))
        stream << ch;
    else {
        stream << "\\\\x" << std::setfill('0') << std::setw(2) << std::hex
               << (static_cast<int>(ch) & 0xFF);
    }

    return stream.str();
}

void ranges(std::basic_ostream<char>& out, boost::icl::interval_set<char> range) {
    bool first = true;
    for (auto&& interval : range) {
        if (!first) {
            out << ",";
        }
        if (interval.lower() == interval.upper())
            out << represent_char(interval.upper());
        else
            out << represent_char(interval.lower()) << "-" << represent_char(interval.upper());
        first = false;
    }
}

int graphviz_codegen(
    std::basic_ostream<char>& out,
    const supercomplex::lexer<char, t_info>& automaton
) {
    out << "digraph {" << std::endl;

    int index = 0;
    const auto& states = automaton.states();

    for (size_t i = 0; i < states.size(); ++i) {
        const auto& state = states[i];

        if (i == automaton.start()) {
            out << "    " << i << " [shape=box,label=\"START\"];" << std::endl;
        } else if (state.terminal) {
            if (state.terminal_info.skip) {
                out << "    " << i << " [label=\"SKIP\", peripheries=2, style=dotted];"
                    << std::endl;
            } else {
                out << "    " << i << " [label=\"" << state.terminal_info.name
                    << "\", peripheries=2];" << std::endl;
            }
        }
    }

    for (size_t i = 0; i < states.size(); ++i) {
        const auto& state = states[i];

        for (auto&& transition : state.transitions) {
            out << "    " << i << " -> " << transition.next << " [label=\"";
            ranges(out, transition.characters);
            out << "\"];" << std::endl;
        }
    }

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
      This will generate dot file for a JSON lexer.
    */
    lexer_generator<char, t_info> lex_gen;
    lex_gen
        << prod("ARR_OPEN", "\\[") << prod("ARR_CLOSE", "\\]") << prod("OBJ_OPEN", "{")
        << prod("OBJ_CLOSE", "}") << prod("LITERAL", "true|false|null") << prod("COMMA", ",")
        << prod("COLON", ":")
        << prod(
               "STRING",
               "\"(\\\\([\"\\\\/bfrnt]|u[0-9a-fA-F][0-9a-fA-F][0-9a-fA-F][0-9a-fA-F])|[^\"\\\\\0-\x1f])*\""s
           )
        << prod("NUMBER", "-?(0|[1-9][0-9]*)(.[0-9]+)?([Ee][+\\-]?(0|[1-9][0-9]*))?")
        << skip("[ \t\n\r]+");

    auto lexer = lex_gen.generate();
    graphviz_codegen(std::cout, lexer);
}
