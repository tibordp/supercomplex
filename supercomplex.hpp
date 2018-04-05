#include <unordered_map>
#include <unordered_set>

#include "dfa.hpp"
#include "nfa.hpp"

#pragma once

namespace supercomplex
{

	template <typename AdditionalInfo>
	struct basic_token_info
	{
		int precedence;
		AdditionalInfo additional_info;

		friend bool operator<(const basic_token_info& lhs,
		    const basic_token_info& rhs)
		{
			return (lhs.precedence < rhs.precedence);
		}

		friend bool operator==(const basic_token_info& lhs1,
		    const basic_token_info& rhs1)
		{
			return lhs1.precedence == rhs1.precedence;
		}

		friend bool operator!=(const basic_token_info& lhs1,
		    const basic_token_info& rhs1)
		{
			return !(lhs1 == rhs1);
		}
	};

	template <typename CharType, typename AdditionalInfo>
	struct lexer_transition
	{
		boost::icl::interval_set<CharType> characters;
		size_t next;

		explicit lexer_transition(size_t next_) : next(next_)
		{
		}
		lexer_transition(boost::icl::interval_set<CharType> characters_,
		    size_t next_)
		  : characters(characters_), next(next_)
		{
		}
	};

	template <typename CharType, typename AdditionalInfo>
	struct lexer_node
	{
		std::vector<lexer_transition<CharType, AdditionalInfo>> transitions;

		bool terminal;
		AdditionalInfo terminal_info;

		lexer_node(const dfa_node<CharType, basic_token_info<AdditionalInfo>>& node)
		  : terminal(node.terminal()),
		    terminal_info(node.get_terminal().additional_info)
		{
		}
	};

	template <typename CharType, typename AdditionalInfo>
	class lexer
	{
		using lexer_node_type = lexer_node<CharType, AdditionalInfo>;
		std::vector<lexer_node_type> _states;
		int _start;

	public:
		template <typename Iterator>
		lexer(Iterator begin, Iterator end, int start)
		  : _states(begin, end), _start(start)
		{
		}

		const std::vector<lexer_node_type>& states() const
		{
			return _states;
		}

		int start() const
		{
			return _start;
		}
	};

	template <typename CharType, typename AdditionalInfo>
	struct lexer_production
	{
		std::basic_string<CharType> regex;
		AdditionalInfo node;

		lexer_production(AdditionalInfo node_,
		    const std::basic_string<CharType>& regex_)
		  : regex(regex_), node(node_){};
	};

	template <typename CharType, typename AdditionalInfo>
	class lexer_generator
	{
		using token_info_type = basic_token_info<AdditionalInfo>;
		using terminal_node_type = terminal_node<CharType, token_info_type>;

		std::vector<terminal_node_type> productions;
		int seq_number;

	public:
		lexer_generator() : seq_number(0){};

		friend lexer_generator& operator<<(lexer_generator& lhs,
		    const lexer_production<CharType, AdditionalInfo>& terminal)
		{
			token_info_type info;
			info.precedence = lhs.seq_number++;
			info.additional_info = terminal.node;
			lhs.productions.emplace_back(terminal_node_type(info, terminal.regex));
			return lhs;
		}

		lexer<CharType, AdditionalInfo> generate()
		{
			nfa<CharType, token_info_type> nfa_machine(
			    productions.begin(), productions.end());
			dfa<CharType, token_info_type> dfa_machine(nfa_machine.start());
			dfa_machine.optimize();

			std::vector<lexer_node<CharType, AdditionalInfo>> nodes;
			std::unordered_map<const dfa_node<CharType, token_info_type>*, int> index;

			for (const auto& state : dfa_machine.states())
			{
				index[state] = nodes.size();
				nodes.emplace_back(*state);
			}

			for (const auto& state : dfa_machine.states())
			{
				for (const auto& transition : state->transitions)
				{
					nodes[index[state]].transitions.emplace_back(
					    transition.characters, index[transition.next]);
				}
			}

			return lexer<CharType, AdditionalInfo>(
			    nodes.begin(), nodes.end(), index[dfa_machine.start()]);
		}
	};
}
