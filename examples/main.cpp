#include <ostream>
#include <iostream>
#include <string>

#include "supercomplex.hpp"

using namespace std;
using namespace supercomplex;

struct t_info {
  std::string name;
  bool skip;
};

void ranges(std::basic_ostream<char>& out, const std::basic_string<char>& name, boost::icl::interval_set<char> range)
{
	if (boost::icl::interval_count(range) > 1) out << "(";
	bool first = true;
	for (auto&& interval : range)
	{
		if (!first) { out << " || "; }
		if (interval.lower() == interval.upper())
			out << "(" << name << " == '" << (char)interval.upper() << "')";
		else
			out << "(" << name << " >= '" << (char)interval.lower() << "' && '" << name << " <= '" << (char)interval.upper() << "')";
		first = false;
	}
	if (boost::icl::interval_count(range) > 1) out << ")";
}

int codegen(std::basic_ostream<char>& out, const supercomplex::lexer<char, t_info>& automaton)
{
	out << "#include <stdio.h>" << std::endl;
	out << "int main() {" << std::endl;
	out << "  int state = " << automaton.start() << ";" << std::endl;
	out << "  char buf[1024];" << std::endl;
	out << "  int buf_pos = 0;" << std::endl;
	out << "  for (;;) {" << std::endl;
	out << "    char ch = getc(stdin);" << std::endl;
	out << "    switch (state) {" << std::endl;

  const auto& states = automaton.states();

	for (size_t i = 0; i < states.size(); ++i)
	{
    const auto& state = states[i];

		out << "      case " << i << ":" << std::endl;
		bool first = true;
		for (auto&& transition : state.transitions)
		{
			if (first) out << "        if ";
			else       out << "        else if ";
			ranges(out, "ch", transition.characters);
			out << std::endl <<  "          state = " << transition.next << ";" << std::endl;
			first = false;
		}
		if (!first)  out << "        else {" << std::endl;
		if (state.terminal)
		{
			auto terminal_node = state.terminal_info;

			out << "          ungetc(ch, stdin);" << std::endl;  // We have to re-evaluate current character.
			out << "          buf[buf_pos] = '\\0';" << std::endl;
			if (!terminal_node.skip)
				out << "          printf(\"<%s, \\\"%s\\\">\", \"" << terminal_node.name << "\", buf);" << std::endl;
			out << "          state = buf_pos = 0;" << std::endl;
			out << "          continue;" << std::endl;
		}
		else
		{
			out << "          goto fail;" << std::endl;
		}
		if (!first)  out << "        }" << std::endl;

		out << "      break;" << std::endl;
	}
	out << "    }" << std::endl;
	out << "    buf[buf_pos++] = ch;" << std::endl;
	out << "    if (buf_pos >= 1024) { fputs(\"Token too long.\", stderr);  return -1; }" << std::endl;
	out << "    if (feof(stdin)) { return 0; }" << std::endl;
	out << "  }" << std::endl;

	out << "  fail:" << std::endl; // If we reached an EOF, we don't care that the EOF character could not match
	out << "  if (feof(stdin)) { return 0; }" << std::endl;
	out << "  fputs(\"Invalid token\", stderr);" << std::endl;
	out << "  return -1;" << std::endl;
	out << "}" << std::endl;
	return 0;
}

lexer_production<char, t_info> prod(const std::string& name, const std::string& regex) {
  return { {name, false}, regex};
}

lexer_production<char, t_info> skip(const std::string& regex) {
  return { {std::string(), true}, regex };
}

int main() {
  lexer_generator<char, t_info> lex_gen;
  lex_gen << skip("g*");
          //<< prod("bar", "foo")
          //<< prod("quux", "foo*");

  auto lexer = lex_gen.generate();
  codegen(std::cout, lexer);

}
