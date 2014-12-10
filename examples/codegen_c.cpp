#include <ostream>
#include <boost/icl/interval_set.hpp>
#include <boost/bimap.hpp>
#include <iostream>

#include "dfa.h"
#include "nfa.h"

// This is the sruct that holds the information about our accepting states in DFA
// it can contain whatever fields, but must be <-comparable so that the minimal 
// one can be chosen in case multiple tokens are matched and ==-comparable so that they
// can be distinguished 
struct c_token
{
	int precedence;
	std::string name;
	bool skip;

	friend bool operator<(const c_token& lhs, const c_token& rhs)
	{
		if (lhs.precedence < rhs.precedence)
			return true;
		if (rhs.precedence < lhs.precedence)
			return false;
		return lhs.name < rhs.name;
	}

	friend bool operator==(const c_token& lhs1, const c_token& rhs1)
	{
		return lhs1.precedence == rhs1.precedence
			&& lhs1.name == rhs1.name;
	}

	friend bool operator!=(const c_token& lhs1, const c_token& rhs1)
	{
		return !(lhs1 == rhs1);
	}
};

void ranges(std::basic_ostream<char>& out, const std::basic_string<char>& name, boost::icl::interval_set<char> range)
{
	if (boost::icl::interval_count(range) > 1) out << "(";
	bool first = true;
	for (auto&& interval : range)
	{
		if (!first) { out << " || "; }
		if (interval.lower() == interval.upper())
			out << "(" << name << " == " << (long long)interval.upper() << ")";
		else
			out << "(" << name << " >= " << (long long)interval.lower() << " && " << name << " <= " << (long long)interval.upper() << ")";
		first = false;
	}
	if (boost::icl::interval_count(range) > 1) out << ")";
}

int codegen(std::basic_ostream<char>& out, const supercomplex::dfa<char, c_token>& automaton)
{
	// We use a bimap so that we can give nice descriptive numeric labels to states.
	boost::bimap<supercomplex::dfa_node<char, c_token>*, int> state_names;

	int i = 1;

	for (auto&& state : automaton.states())
	{
		if (state == &automaton.start())
			state_names.insert({ state, 0 });
		else
			state_names.insert({ state, i++ });
	}
	
	out << "#include <stdio.h>" << std::endl;
	out << "int main() {" << std::endl;
	out << "  int state = 0;" << std::endl;	
	out << "  char buf[1024];" << std::endl;
	out << "  int buf_pos = 0;" << std::endl;
	out << "  for (;;) {" << std::endl;
	out << "    char ch = getc(stdin);" << std::endl;
	out << "    switch (state) {" << std::endl;

	for (auto&& state : state_names.right)
	{
		out << "      case " << state.first << ":" << std::endl;
		bool first = true;
		for (auto&& transition : state.second->transitions)
		{
			if (first) out << "        if ";
			else       out << "        else if ";
			ranges(out, "ch", transition.characters);
			out << std::endl <<  "          state = " << state_names.left.at(transition.next) << ";" << std::endl;
			first = false;
		}
		if (!first)  out << "        else {" << std::endl;
		if (state.second->terminal())
		{
			auto terminal_node = state.second->get_terminal();
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

int main()
{
	supercomplex::nfa<char, c_token> nfa_machine
	{ 
		{{ 0, "Integer", false }, "[0-9]+"},
		{{ 1, "Float", false }, "[0-9]+.[0-9]*|[0-9]*.[0-9]+"},
		{{ 2, "Plus", false }, "\\+" },
		{{ 3, "Minus", false }, "\\-" },
		{{ 4, "Times", false }, "\\*" },
		{{ 5, "Divided", false }, "/" },
		{{ 6, "OpenParen", false }, "\\(" },
		{{ 7, "CloseParen", false }, "\\)" },
		{{ 8, "Whitespace", true }, "[ \r\n\t]+"},
	};

	supercomplex::dfa<char, c_token> dfa_machine(nfa_machine.start());
	dfa_machine.optimize();
	codegen(std::cout, dfa_machine);

	return 0;
}