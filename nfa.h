/*
Copyright (c) 2014 Tibor Djurica Potpara <tibor.djurica@ojdip.net>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*/

#pragma once

#include <vector>
#include <memory>
#include <boost/icl/interval_set.hpp>

namespace supercomplex {

	template <typename CharType, typename TokenInfo>
	struct nfa_node;

	template <typename CharType, typename TokenInfo>
	struct nfa_transition
	{
		bool epsilon;

		boost::icl::interval_set<CharType> characters;
		nfa_node<CharType, TokenInfo>* next;

		explicit nfa_transition(nfa_node<CharType, TokenInfo>* next_)
			: epsilon(true), next(next_) {}

		nfa_transition(boost::icl::interval_set<CharType> characters_, nfa_node<CharType, TokenInfo>* next_)
			: epsilon(false), characters(characters_), next(next_) {}
	};

	template <typename CharType, typename TokenInfo>
	struct nfa_node
	{
		bool terminal;
		TokenInfo token;
		std::vector<nfa_transition<CharType, TokenInfo>> transitions;

		nfa_node() : terminal(false)
		{
		}
	};

	template <typename CharType, typename TokenInfo>
	struct nfa_segment
	{
		using node_type = nfa_node < CharType, TokenInfo > ;

		node_type* begin;
		node_type* end;

		nfa_segment(node_type* begin, node_type* end)
			: begin(begin),
			end(end)
		{
		}
	};

	template <typename CharType, typename TokenInfo>
	struct terminal_node
	{
		TokenInfo token;
		std::basic_string<CharType> regex;

		terminal_node(const TokenInfo& token, const CharType* regex)
			: token(token),
			regex(regex)
		{
		}
	};

	template<typename CharType, typename TokenInfo>
	struct regex_node
	{
		virtual ~regex_node() { }
		virtual nfa_segment<CharType, TokenInfo> nfa() = 0;
	};

	enum class operator_type { plus, star, optional };

	template<typename CharType, typename TokenInfo>
	struct character_set : regex_node < CharType, TokenInfo >
	{
		boost::icl::interval_set<CharType> char_set;
		character_set() {};
		explicit character_set(CharType a)
		{
			char_set.insert(a);
		};

		nfa_segment<CharType, TokenInfo> nfa() override
		{
			nfa_segment<CharType, TokenInfo> result(new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>());
			result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(char_set, result.end));
			return result;
		}
	};

	template<typename CharType, typename TokenInfo>
	struct operand : regex_node <CharType, TokenInfo>
	{
		std::shared_ptr<regex_node<CharType, TokenInfo>> child;
		operator_type oper;
		operand(std::shared_ptr<regex_node<CharType, TokenInfo>> child_, operator_type oper_)
			: child(child_), oper(oper_)
		{}

		nfa_segment<CharType, TokenInfo> nfa() override
		{
			nfa_segment<CharType, TokenInfo> child_nfa = child->nfa();
			nfa_segment<CharType, TokenInfo> result(new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>());
			switch (oper)
			{
			case operator_type::plus:
				child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(child_nfa.begin));
				break;
			case operator_type::star:
				result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end));
				child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(child_nfa.begin));
				break;
			case operator_type::optional:
				result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end));
				break;
			default: break;
			}
			result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(child_nfa.begin));
			child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end));
			return result;
		}
	};

	template<typename CharType, typename TokenInfo>
	struct concatenate : regex_node < CharType, TokenInfo >
	{
		std::vector<std::shared_ptr<regex_node<CharType, TokenInfo>>> terms;

		nfa_segment<CharType, TokenInfo> nfa() override
		{
			nfa_node<CharType, TokenInfo>* first = new nfa_node<CharType, TokenInfo>();
			nfa_node<CharType, TokenInfo>* last = first;

			for (auto&& node : terms)
			{
				nfa_segment<CharType, TokenInfo> child_nfa = node->nfa();
				for (auto&& transition : child_nfa.begin->transitions)
				{
					last->transitions.push_back(transition);
				}
				last = child_nfa.end;
			}

			return nfa_segment<CharType, TokenInfo>(first, last);
		}
	};

	template<typename CharType, typename TokenInfo>
	struct alternative : regex_node < CharType, TokenInfo >
	{
		std::vector<std::shared_ptr<regex_node<CharType, TokenInfo>>> alternatives;

		nfa_segment<CharType, TokenInfo> nfa() override
		{
			nfa_segment<CharType, TokenInfo> result(new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>());

			for (auto&& node : alternatives)
			{
				nfa_segment<CharType, TokenInfo> child_nfa = node->nfa();
				result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(child_nfa.begin));
				child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end));
			}

			return result;
		}
	};

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_regex(T& begin, T end);

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_char_range(T& begin, T end, bool complement)
	{
		using namespace boost::icl;
		std::shared_ptr<character_set<CharType, TokenInfo>> expr(new character_set<CharType, TokenInfo>());
		CharType last;

		if (complement)
			expr->char_set.add(construct<discrete_interval<CharType>>
			(std::numeric_limits<CharType>::min(),
			std::numeric_limits<CharType>::max(),
			interval_bounds::closed()));

		enum class states { normal, range, escape, escape_range };

		auto state = states::normal;

		while (begin != end)
		{
			switch (state)
			{
			case states::normal:
				if (*begin == ']')
					return expr;
				if (*begin == '\\')
				{
					state = states::escape; break;
				}
				if (*begin == '-')
				{
					state = states::range; break;
				} // no break;
			case states::escape:
				last = *begin;
				if (complement)
					expr->char_set.erase(last);
				else
					expr->char_set.insert(last);
				state = states::normal;
				break;
			case states::range:
				if (*begin == '\\')
				{
					state = states::escape_range; break;
				} // no break;
			case states::escape_range:
				auto interval = boost::icl::construct<discrete_interval<CharType>>(last, *begin, interval_bounds::closed());
				if (complement)
					expr->char_set.erase(interval);
				else
					expr->char_set.insert(interval);
				state = states::normal;
				break;
			}
			++begin;
		}

		throw new std::runtime_error("Invalid regex.");
	}

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_atom(T& begin, T end)
	{
		if (*begin == '?' || *begin == '*' || *begin == ')' || *begin == '+' || *begin == '|')
			return nullptr;
		if (*begin == '(')
		{
			++begin;
			auto node = parse_regex<CharType, TokenInfo>(begin, end);
			if (*begin != ')') throw new std::runtime_error("Invalid regex.");
			++begin;
			return node;
		}
		if (*begin == '[')
		{
			++begin;
			bool complement = false;
			if (*begin == '^')
			{
				complement = true;
				++begin;
			}
			auto node = parse_char_range<CharType, TokenInfo>(begin, end, complement);
			if (*begin != ']') throw new std::runtime_error("Invalid regex.");
			++begin;
			return node;
		}
		if (*begin == '\\')
		{
			++begin;
		}
		return std::make_shared<character_set<CharType, TokenInfo>>(*begin++);
	}

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_term(T& begin, T end)
	{
		auto char_range = parse_atom<CharType, TokenInfo>(begin, end);

		if (char_range.get() == nullptr) return nullptr;
		if (begin == end)
			return char_range;
		switch (*begin++)
		{
		case '+': return std::make_shared<operand<CharType, TokenInfo>>(char_range, operator_type::plus);
		case '*':  return std::make_shared<operand<CharType, TokenInfo>>(char_range, operator_type::star);
		case '?':  return std::make_shared<operand<CharType, TokenInfo>>(char_range, operator_type::optional);
		default: {
			--begin;
			return char_range;
		}
		}
	}

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_factor(T& begin, T end)
	{
		std::shared_ptr<concatenate<CharType, TokenInfo>> expr(new concatenate<CharType, TokenInfo>());

		expr->terms.push_back(parse_term<CharType, TokenInfo>(begin, end));

		while (begin != end)
		{
			auto new_term = parse_term<CharType, TokenInfo>(begin, end);
			if (new_term.get() == nullptr) return expr;
			expr->terms.push_back(new_term);
		}

		return expr;
	}

	template<typename CharType, typename TokenInfo, typename T>
	std::shared_ptr<regex_node<CharType, TokenInfo>> parse_regex(T& begin, T end)
	{
		std::shared_ptr<alternative<CharType, TokenInfo>> expr(new alternative<CharType, TokenInfo>());

		expr->alternatives.push_back(parse_factor<CharType, TokenInfo>(begin, end));

		while (begin != end && *begin == '|')
		{
			++begin;
			expr->alternatives.push_back(parse_factor<CharType, TokenInfo>(begin, end));
		}

		return expr;
	}


	template <typename CharType, typename TokenInfo>
	class nfa
	{
		using node_type = nfa_node < CharType, TokenInfo > ;

		node_type* _start;

		nfa(const nfa& other) = delete;
		nfa& operator=(const nfa&) = delete;
	public:
		nfa(const std::basic_string<CharType>& regex, TokenInfo token_info)
		{
			auto begin = regex.cbegin();
			auto regex_node = parse_regex<CharType, TokenInfo>(begin, regex.end());
			auto thunk = regex_node->nfa();
			thunk.end->terminal = true;
			thunk.end->token = token_info;
			_start = thunk.begin;
		}

		nfa(const std::initializer_list<terminal_node<CharType, TokenInfo>>& entries)
		{
			_start = new node_type();
			for (auto && entry : entries)
			{
				auto begin = entry.regex.cbegin();
				auto regex_node = parse_regex<CharType, TokenInfo>(begin, entry.regex.end());
				auto thunk = regex_node->nfa();
				thunk.end->terminal = true;
				thunk.end->token = entry.token;
				_start->transitions.emplace_back(thunk.begin);
			}
		}

		const node_type& start() const
		{
			return *_start;
		}

		~nfa()
		{
			std::unordered_set<node_type*> visited;
			std::queue<node_type*> to_visit;

			to_visit.push(_start);
			visited.insert(_start);

			while (!to_visit.empty())
			{
				for (auto&& transition : to_visit.front()->transitions)
				{
					if (visited.find(transition.next) == visited.end())
					{
						to_visit.push(transition.next);
						visited.insert(transition.next);
					}
				}
				delete to_visit.front();
				to_visit.pop();
			}
		}
	};
}