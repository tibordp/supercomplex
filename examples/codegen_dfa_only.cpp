#include <ostream>
#include <boost/icl/interval_set.hpp>
#include <boost/bimap.hpp>
#include <iostream>
#include <string>

#include "dfa.h"
#include "nfa.h"

// This is the struct that holds the information about our accepting states in DFA
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

/*
* This method is for pretty-printing the set of letters of a state transition
* e.g. in the form of [a-zA-Z12345678]
* */
std::string interval_repr(const boost::icl::interval_set<char>& letters)
{
	std::stringstream ss;
	ss << "[";
	for(auto interval : letters)
	{
		if (std::isprint(interval.lower()))
			ss << interval.lower();
		else
			ss << "\\" << (unsigned) interval.lower();

		if (interval.lower() == interval.upper()) continue;

		ss << "-";

		if (std::isprint(interval.upper()))
			ss << interval.upper();
		else
			ss << "\\" << (unsigned) interval.upper();
	}
	ss << "]";
	return ss.str();
};

int main()
{
	supercomplex::nfa<char, c_token> nfa_machine
	{ 
		{{ 0,   "Integer",    false }, "[0-9]+"},
		{{ 1,   "Float",      false }, "[0-9]+.[0-9]*(e[\\+\\-]?[0-9]+)?|[0-9]*.[0-9]+(e[\\+\\-]?[0-9]+)?"},
		{{ 2,   "Plus",       false }, "\\+" },
		{{ 3,   "Minus",      false }, "\\-" },
		{{ 4,   "Times",      false }, "\\*" },
		{{ 5,   "Divided",    false }, "/" },
		{{ 6,   "OpenParen",  false }, "\\(" },
		{{ 7,   "CloseParen", false }, "\\)" },
		{{ 8,   "OpenParen",  false }, "\\(" },
		{{ 9,   "CloseParen", false }, "\\)" },
		{{ 100, "Identifier", false }, "[a-zA-Z_][a-zA-Z0-9_]*|some|specific|identifiers"},
		{{ 200, "Whitespace", true  }, "[ ]+"},
	};

	supercomplex::dfa<char, c_token> dfa_machine(nfa_machine.start());
	/*
	 * The following method is used to give a name to the states we encounter.
	 * */
	std::map<const supercomplex::dfa_node<char, c_token>*, int> names;
	int i = 1;

	auto state_name = [&] (const supercomplex::dfa_node<char, c_token>* state) -> std::string
	{
		std::stringstream ss;

		if (state == &dfa_machine.start()) return "START";
		auto it = names.find(state);

		if (state->terminal()) ss << "[" << state->get_terminal().name; // Print the accepting state's token name
		if (names.find(state) != names.end())
		{
			ss << it->second ;
		}
		else
		{
			names.emplace(state, i);
			ss << i++ ;
		}
		if (state->terminal()) ss << "]";
		return ss.str();
	};

	/*
	 * Finally we loop through states and print their transitions.
	 * */
	std::cout << "Unoptimized!" << std::endl;
	std::cout << "============" << std::endl;

	for (auto state : dfa_machine.states())
	{
		auto name = state_name(state);
		for (auto transition : state->transitions)
		{
			std::cout << name << "\t=" << interval_repr(transition.characters) << "=>\t" << state_name(transition.next) << std::endl;
		}
	}

	dfa_machine.optimize();

	std::cout << std::endl << "Optimized!" << std::endl;
	std::cout << "==========" << std::endl;

	for (auto state : dfa_machine.states())
	{
		auto name = state_name(state);
		for (auto transition : state->transitions)
		{
			std::cout << name << "\t=" << interval_repr(transition.characters) << "=>\t" << state_name(transition.next) << std::endl;
		}
	}

	return 0;
}