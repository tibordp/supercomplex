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

#include <boost/icl/interval_set.hpp>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <vector>

#include "nfa.h"

namespace supercomplex {

	template <typename CharType, typename TokenInfo>
	struct dfa_node;

	template <typename CharType, typename TokenInfo>
	struct dfa_transition
	{
		using node_type = dfa_node < CharType, TokenInfo > ;

		boost::icl::interval_set<CharType> characters;
		node_type* next;

		explicit dfa_transition(node_type* next_)
			: next(next_) {}

		dfa_transition(boost::icl::interval_set<CharType> characters_, node_type* next_)
			: characters(characters_), next(next_) {}
	};

	template <typename CharType, typename TokenInfo>
	struct dfa_node
	{
		using nfa_node_type = nfa_node < CharType, TokenInfo > ;

		std::unordered_set<const nfa_node_type*> nodes;
		std::vector<dfa_transition<CharType, TokenInfo>> transitions;

		explicit dfa_node(const std::unordered_set<const nfa_node_type*>& nfa_nodes)
			: nodes(nfa_nodes)
		{
		}

		bool terminal() const
		{
			for (auto&& node : nodes)
			{
				if (node->terminal) return true;
			}
			return false;
		}

		TokenInfo get_terminal() const
		{
			std::vector<TokenInfo> ti;
			for (auto&& nfa_node : nodes)
			{
				if (nfa_node->terminal) ti.push_back(nfa_node->token);
			}

			if (ti.empty()) return TokenInfo();
			return *std::min_element(ti.begin(), ti.end());
		}

		dfa_node()
		{
		}
	};

	/**
	* Returns all states reachable from any of the input NFA states by following a transition containing 
	* specified interval (this is a generalization of move(T, a) for alphabet symbol a to intervals of symbosl)
	*/
	template<typename T, typename CharType, typename TokenInfo>
	void move_s(std::unordered_set<const nfa_node<CharType, TokenInfo>*>& dest, const T& nodes, const boost::icl::discrete_interval<CharType>& chars)
	{
		for (auto&& node : nodes)
		{
			for (auto&& trans : node->transitions)
			{
				if (boost::icl::contains(trans.characters, chars))
				{
					dest.insert(trans.next);
				}
			}
		}
	}

	/**
	* Computes the epsilon-closure of a set of NFA states i.e. all states reachable from them following
	* only epsilon transitions.
	*/
	template<typename CharType, typename TokenInfo, typename T>
	void eclosure(std::unordered_set<const nfa_node<CharType, TokenInfo>*>& dest, const T& nodes)
	{
		std::queue<const nfa_node<CharType, TokenInfo>*> to_visit;

		for (auto&& node : nodes)
		{
			to_visit.push(node);
			dest.insert(node);
		}

		while (!to_visit.empty())
		{
			for (auto&& trans : to_visit.front()->transitions)
			{
				if (trans.epsilon)
				{
					if (dest.find(trans.next) == dest.end())
					{
						to_visit.push(trans.next);
						dest.insert(trans.next);
					}
				}
			}
			to_visit.pop();
		}
	}
	
	/**
	* Takes a collection of interval sets and splits their union into disjoint intervals corresponding to all
	* possible intersections between collections.
	*/
	template<typename CharType, typename T>
	void make_disjoint(std::set<boost::icl::discrete_interval<CharType>>& dest, const T& nodes)
	{
		std::vector<std::pair<bool, CharType>> vec;
		for (auto&& node : nodes)
		{
			for (auto&& trans : node->transitions)
			{
				for (auto&& inter : trans.characters)
				{
					vec.push_back({ true, inter.lower() });
					vec.push_back({ false, inter.upper() });
				}
			}
		}
		sort(vec.begin(), vec.end(), [](std::pair<bool, CharType> a, std::pair<bool, CharType> b)
		{
			return a.second < b.second ||
				((a.second == b.second) && (a.first && !b.first));
		});

		int depth = 0;
		std::pair<bool, CharType> last;

		for (auto current : vec)
		{
			if (depth != 0)
			{
				auto lower = last.second + (last.first ? 0 : 1);
				auto upper = current.second - (current.first ? 1 : 0);
				if (lower <= upper)
				{
					dest.insert(boost::icl::construct<boost::icl::discrete_interval<CharType>>
						(lower, upper, boost::icl::interval_bounds::closed()));
				}
			}

			last = current;

			if (current.first)  { depth++; }
			if (!current.first) { depth--; }
		}
	}

	template<typename CharType, typename TokenInfo>
	class dfa {
	public:
		using node_type = dfa_node < CharType, TokenInfo > ;
		using nfa_node_type = nfa_node < CharType, TokenInfo > ;
		using set_type = std::unordered_set < dfa_node<CharType, TokenInfo>* > ;
	private:
		set_type _nodes;
		node_type* _initial;

		dfa(const dfa& other) = delete;
		dfa& operator=(const dfa&) = delete;

		struct dfa_set_hash
		{
			size_t operator()(const set_type & x) const
			{
				size_t hash = 2166136261;
				for (auto&& part : x)
					hash = hash * 16777619 ^ reinterpret_cast<size_t>(part);
				return hash;
			}
		};

		struct dfa_set_eq
		{
			bool operator()(const set_type& x, const set_type& y) const
			{
				return x == y;
			}
		};

		struct dfa_node_hash
		{
			size_t operator()(node_type* x) const
			{
				size_t hash = 2166136261;
				for (auto&& part : x->nodes)
					hash = hash * 16777619 ^ reinterpret_cast<size_t>(part);
				return hash;
			}
		};

		struct dfa_node_eq
		{
			bool operator()(node_type* x, node_type* y) const
			{
				return x->nodes == y->nodes;
			}
		};

		void aggregate()
		{
			for (auto& node : _nodes)
			{
				std::sort(node->transitions.begin(), node->transitions.end(),
					[](const dfa_transition<CharType, TokenInfo>& a, const dfa_transition<CharType, TokenInfo>& b)
				{
					return a.next < b.next;
				});

				for (size_t i = 1; i < node->transitions.size(); ++i)
				{
					if (node->transitions[i].next == node->transitions[i - 1].next)
					{
						for (auto&& inter : node->transitions[i].characters)
							node->transitions[i - 1].characters.add(inter);
						node->transitions.erase(node->transitions.begin() + i);
						--i;
					}
				}
			}
		}

		/**
		* Part of the Moore's algorithm for DFA state minimization. Check whether two states are equivalent under a 
		* specified equivalence relation (for each input symbol a, the a-transitions of both states lead to the same
		* element of the set partition.
		*/
		bool check_equivalence(const node_type* state1, const node_type* state2, const std::unordered_map<node_type*, const set_type*>& Map)
		{
			if (state1 == state2) return true;
			std::set<boost::icl::discrete_interval<CharType>> letters;
			make_disjoint<CharType>(letters, std::initializer_list < const node_type* > { state1, state2 });

			for (auto&& interval : letters)
			{
				node_type* next1 = nullptr;
				node_type* next2 = nullptr;

				for (const auto& tran : state1->transitions)
				{
					if (boost::icl::contains(tran.characters, interval))
					{
						next1 = tran.next;
					}
				}

				for (const auto& tran : state2->transitions)
				{
					if (boost::icl::contains(tran.characters, interval))
					{
						next2 = tran.next;
					}
				}

				if ((next1 == nullptr) ^ (next2 == nullptr)) return false;
				if ((next1 == nullptr) && (next2 == nullptr)) continue;
				if (Map.at(next1) != Map.at(next2)) return false;
			}

			return true;
		}
	public:
		dfa(const nfa_node_type& start)
		{
			auto initial = new node_type();

			std::unordered_set<node_type*> unmarked;
			std::unordered_set<node_type*, dfa_node_hash, dfa_node_eq> result;
			std::unordered_set<const nfa_node_type*> moved;

			auto start_nodes{ &start };
			eclosure(initial->nodes, start_nodes);

			result.insert(initial);
			unmarked.insert(initial);

			while (!unmarked.empty())
			{
				auto it = unmarked.begin();
				auto D = *it;

				unmarked.erase(it);

				std::set<boost::icl::discrete_interval<CharType>> letters;
				make_disjoint<CharType>(letters, D->nodes);

				for (auto && inter : letters)
				{
					auto new_node = new node_type();
					moved.clear();
					move_s(moved, D->nodes, inter);
					eclosure(new_node->nodes, moved);

					bool inserted = false;
					if (!new_node->nodes.empty())
					{
						auto status = result.emplace(new_node);
						if (status.second)
						{
							unmarked.insert(new_node);
							inserted = true;
						}

						boost::icl::interval_set<CharType> iset;
						iset.add(inter);
						D->transitions.emplace_back(iset, *status.first);
					}

					if (!inserted) delete new_node;
				}
			}
			_nodes = set_type(result.begin(), result.end());
			_initial = initial;
		}

		/**
		* Moore's algorithm for DFA state minimization. Identifies all the indistinguishable subsets of
		* DFA states and replaces them with a single state.
		*/
		void optimize(bool aggregate_intervals)
		{
			// Compilers: Principles, Techniques and Tools SE, page 182
			std::unordered_set<set_type, dfa_set_hash, dfa_set_eq> Gamma, newGamma;
			std::unordered_map<node_type*, const set_type*> Map, newMap;
			set_type newG;

			// Create the initial partition - to accepting and non-accepting states
			Gamma.insert(_nodes);
			bool initialPartition = true;

			// Refine partitions until there are no more changes.	
			while (true)
			{
				for (auto G : Gamma) //Copy
				{
					while (!G.empty())
					{
						newG.clear();
						node_type* pivot = nullptr;

						for (auto it = G.begin(); it != G.end();) {
							bool equiv;
							if (pivot == nullptr)
								pivot = *it;

							// Initial partition is made by grouping all the terminal states 
							// emiting the same token type (all non-terminals also together).
							if (initialPartition)
								equiv = (pivot->terminal() == (*it)->terminal())
								&& (pivot->get_terminal() == (*it)->get_terminal());
							else
								equiv = check_equivalence(pivot, *it, Map);

							if (equiv) {
								newG.insert(*it);
								G.erase(it++);
							}
							else {
								++it;
							}
						}

						auto subset = &*newGamma.insert(newG).first;

						for (auto it = newG.begin(); it != newG.end(); ++it)
						{
							newMap.emplace(*it, subset);
						}
					}
				}

				initialPartition = false;

				if (newGamma.size() == Gamma.size())
					break;
				else
				{
					newGamma.swap(Gamma);
					newMap.swap(Map);

					newGamma.clear();
					newMap.clear();
				}
			}

			set_type dead_states;

			_initial = *newMap.at(_initial)->begin();

			for (auto& node : _nodes)
			{
				for (auto& transitions : node->transitions)
				{
					auto& equiv_class = *newMap.at(transitions.next);
					auto it = equiv_class.begin();
					transitions.next = *it;
					while (++it != equiv_class.end())
					{
						dead_states.insert(*it);
					}
				}
			}

			for (auto& node : dead_states)
			{
				_nodes.erase(node);
				delete node;
			}

			if (aggregate_intervals) aggregate();
		}

		const node_type& start() const
		{
			return *_initial;
		}

		const set_type& states() const
		{
			return _nodes;
		}

		~dfa()
		{
			for (auto node : _nodes) delete node;
		}
	};
}