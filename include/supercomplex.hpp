#pragma once

#include <memory>
#include <queue>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/icl/interval_set.hpp>

namespace supercomplex {

    template <typename CharType, typename TokenInfo>
    struct nfa_node;

    template <typename CharType, typename TokenInfo>
    struct nfa_transition {
        bool epsilon;

        boost::icl::interval_set<CharType> characters;
        nfa_node<CharType, TokenInfo>* next;

        explicit nfa_transition(nfa_node<CharType, TokenInfo>* next_) : epsilon(true), next(next_) {
        }

        nfa_transition(
            boost::icl::interval_set<CharType> characters_,
            nfa_node<CharType, TokenInfo>* next_
        )
          : epsilon(false), characters(characters_), next(next_) {
        }
    };

    template <typename CharType, typename TokenInfo>
    struct nfa_node {
        bool terminal;
        TokenInfo token;
        std::vector<nfa_transition<CharType, TokenInfo>> transitions;

        nfa_node() : terminal(false) {
        }
    };

    template <typename CharType, typename TokenInfo>
    struct nfa_segment {
        using node_type = nfa_node<CharType, TokenInfo>;

        node_type* begin;
        node_type* end;

        nfa_segment(node_type* begin, node_type* end) : begin(begin), end(end) {
        }
    };

    template <typename CharType, typename TokenInfo>
    struct terminal_node {
        TokenInfo token;
        std::basic_string<CharType> regex;

        terminal_node(const TokenInfo& token, const std::string& regex)
          : token(token), regex(regex) {
        }
    };

    template <typename CharType, typename TokenInfo>
    struct regex_node {
        virtual ~regex_node() {
        }
        virtual nfa_segment<CharType, TokenInfo> nfa() = 0;
    };

    enum class operator_type { plus, star, optional };

    template <typename CharType, typename TokenInfo>
    struct character_set : regex_node<CharType, TokenInfo> {
        boost::icl::interval_set<CharType> char_set;
        character_set(){};
        explicit character_set(CharType a) {
            char_set.add(a);
        };

        nfa_segment<CharType, TokenInfo> nfa() override {
            nfa_segment<CharType, TokenInfo> result(
                new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>()
            );
            result.begin->transitions.push_back(
                nfa_transition<CharType, TokenInfo>(char_set, result.end)
            );
            return result;
        }
    };

    template <typename CharType, typename TokenInfo>
    struct operand : regex_node<CharType, TokenInfo> {
        std::shared_ptr<regex_node<CharType, TokenInfo>> child;
        operator_type oper;
        operand(std::shared_ptr<regex_node<CharType, TokenInfo>> child_, operator_type oper_)
          : child(child_), oper(oper_) {
            if (child_.get() == nullptr) {
                throw std::runtime_error("Cannot quantify an empty string.");
            }
        }

        nfa_segment<CharType, TokenInfo> nfa() override {
            nfa_segment<CharType, TokenInfo> child_nfa = child->nfa();
            nfa_segment<CharType, TokenInfo> result(
                new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>()
            );
            switch (oper) {
                case operator_type::plus:
                    child_nfa.end->transitions.push_back(
                        nfa_transition<CharType, TokenInfo>(child_nfa.begin)
                    );
                    break;
                case operator_type::star:
                    result.begin->transitions.push_back(
                        nfa_transition<CharType, TokenInfo>(result.end)
                    );
                    child_nfa.end->transitions.push_back(
                        nfa_transition<CharType, TokenInfo>(child_nfa.begin)
                    );
                    break;
                case operator_type::optional:
                    result.begin->transitions.push_back(
                        nfa_transition<CharType, TokenInfo>(result.end)
                    );
                    break;
                default:
                    break;
            }
            result.begin->transitions.push_back(nfa_transition<CharType, TokenInfo>(child_nfa.begin)
            );
            child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end));
            return result;
        }
    };

    template <typename CharType, typename TokenInfo>
    struct concatenate : regex_node<CharType, TokenInfo> {
        std::vector<std::shared_ptr<regex_node<CharType, TokenInfo>>> terms;

        nfa_segment<CharType, TokenInfo> nfa() override {
            nfa_node<CharType, TokenInfo>* first = new nfa_node<CharType, TokenInfo>();
            nfa_node<CharType, TokenInfo>* last = first;

            for (auto&& node : terms) {
                nfa_segment<CharType, TokenInfo> child_nfa = node->nfa();
                for (auto&& transition : child_nfa.begin->transitions) {
                    last->transitions.push_back(transition);
                }
                last = child_nfa.end;
            }

            return nfa_segment<CharType, TokenInfo>(first, last);
        }
    };

    template <typename CharType, typename TokenInfo>
    struct alternative : regex_node<CharType, TokenInfo> {
        std::vector<std::shared_ptr<regex_node<CharType, TokenInfo>>> alternatives;

        nfa_segment<CharType, TokenInfo> nfa() override {
            nfa_segment<CharType, TokenInfo> result(
                new nfa_node<CharType, TokenInfo>(), new nfa_node<CharType, TokenInfo>()
            );

            for (auto&& node : alternatives) {
                nfa_segment<CharType, TokenInfo> child_nfa = node->nfa();
                result.begin->transitions.push_back(
                    nfa_transition<CharType, TokenInfo>(child_nfa.begin)
                );
                child_nfa.end->transitions.push_back(nfa_transition<CharType, TokenInfo>(result.end)
                );
            }

            return result;
        }
    };

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_regex(T& begin, T end);

    template <typename CharType>
    void make_closed(boost::icl::interval_set<CharType>& intervals) {
        using namespace boost::icl;

        interval_set<CharType> new_set;
        for (auto&& interval : intervals) {
            bool left_open =
                (interval.bounds() == interval_bounds::left_open() ||
                 interval.bounds() == interval_bounds::open());
            bool right_open =
                (interval.bounds() == interval_bounds::right_open() ||
                 interval.bounds() == interval_bounds::open());

            new_set.add(construct<discrete_interval<CharType>>(
                interval.lower() + (left_open ? 1 : 0),
                interval.upper() - (right_open ? 1 : 0),
                interval_bounds::closed()
            ));
        }

        std::swap(new_set, intervals);
    }

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_char_range(
        T& begin,
        T end,
        bool complement
    ) {
        using namespace boost::icl;
        std::shared_ptr<character_set<CharType, TokenInfo>> expr(
            new character_set<CharType, TokenInfo>()
        );
        CharType last;

        if (complement)
            expr->char_set.add(construct<discrete_interval<CharType>>(
                std::numeric_limits<CharType>::min(),
                std::numeric_limits<CharType>::max(),
                interval_bounds::closed()
            ));

        enum class states { normal, range, escape, escape_range };

        auto state = states::normal;

        while (begin != end) {
            switch (state) {
                case states::normal:
                    if (*begin == ']') {
                        ++begin;
                        make_closed(expr->char_set);
                        return expr;
                    }
                    if (*begin == '\\') {
                        state = states::escape;
                        break;
                    }
                    if (*begin == '-') {
                        state = states::range;
                        break;
                    } // no break;
                case states::escape:
                    last = *begin;
                    if (complement)
                        expr->char_set.subtract(last);
                    else
                        expr->char_set.add(last);
                    state = states::normal;
                    break;
                case states::range:
                    if (*begin == '\\') {
                        state = states::escape_range;
                        break;
                    } // no break;
                case states::escape_range:
                    auto interval = boost::icl::construct<discrete_interval<CharType>>(
                        last, *begin, interval_bounds::closed()
                    );
                    if (complement)
                        expr->char_set.subtract(interval);
                    else
                        expr->char_set.add(interval);
                    state = states::normal;
                    break;
            }
            ++begin;
        }

        throw std::runtime_error("Invalid regular expression - unterminated char range.");
    }

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_atom(T& begin, T end) {
        if (*begin == ']')
            throw std::runtime_error("Invalid regular expression - unmatched ]");
        if (*begin == '?' || *begin == '*' || *begin == ')' || *begin == '+' || *begin == '|')
            return nullptr;
        if (*begin == '(') {
            ++begin;
            auto node = parse_regex<CharType, TokenInfo>(begin, end);
            if (*begin != ')')
                throw std::runtime_error("Invalid regular expression - unterminated subexpression."
                );
            ++begin;
            return node;
        }
        if (*begin == '[') {
            ++begin;
            bool complement = false;
            if (*begin == '^') {
                complement = true;
                ++begin;
            }
            return parse_char_range<CharType, TokenInfo>(begin, end, complement);
        }
        if (*begin == '\\') {
            ++begin;
        }
        return std::make_shared<character_set<CharType, TokenInfo>>(*begin++);
    }

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_term(T& begin, T end) {
        auto char_range = parse_atom<CharType, TokenInfo>(begin, end);
        if (begin == end)
            return char_range;

        switch (*begin++) {
            case '+':
                return std::make_shared<operand<CharType, TokenInfo>>(
                    char_range, operator_type::plus
                );
            case '*':
                return std::make_shared<operand<CharType, TokenInfo>>(
                    char_range, operator_type::star
                );
            case '?':
                return std::make_shared<operand<CharType, TokenInfo>>(
                    char_range, operator_type::optional
                );
            default:
                --begin;
                return char_range;
        }
    }

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_factor(T& begin, T end) {
        std::shared_ptr<concatenate<CharType, TokenInfo>> expr(new concatenate<CharType, TokenInfo>(
        ));

        while (begin != end) {
            auto new_term = parse_term<CharType, TokenInfo>(begin, end);
            if (new_term.get() == nullptr)
                return expr;
            expr->terms.push_back(new_term);
        }

        return expr;
    }

    template <typename CharType, typename TokenInfo, typename T>
    std::shared_ptr<regex_node<CharType, TokenInfo>> parse_regex(T& begin, T end) {
        std::shared_ptr<alternative<CharType, TokenInfo>> expr(new alternative<CharType, TokenInfo>(
        ));

        expr->alternatives.push_back(parse_factor<CharType, TokenInfo>(begin, end));

        while (begin != end && *begin == '|') {
            ++begin;
            expr->alternatives.push_back(parse_factor<CharType, TokenInfo>(begin, end));
        }

        return expr;
    }

    template <typename CharType, typename TokenInfo>
    class nfa {
        using node_type = nfa_node<CharType, TokenInfo>;

        node_type* _start;

        nfa(const nfa& other) = delete;
        nfa& operator=(const nfa&) = delete;

      public:
        nfa(const std::basic_string<CharType>& regex, TokenInfo token_info) {
            auto begin = regex.begin();
            auto regex_node = parse_regex<CharType, TokenInfo>(begin, regex.end());
            auto thunk = regex_node->nfa();
            thunk.end->terminal = true;
            thunk.end->token = token_info;
            _start = thunk.begin;
        }

        template <class Iterator>
        nfa(Iterator begin, Iterator end) {
            _start = new node_type();
            for (; begin != end; ++begin) {
                auto r_begin = begin->regex.begin();
                auto regex_node = parse_regex<CharType, TokenInfo>(r_begin, begin->regex.end());
                auto thunk = regex_node->nfa();
                thunk.end->terminal = true;
                thunk.end->token = begin->token;
                _start->transitions.emplace_back(thunk.begin);
            }
        }

        const node_type& start() const {
            return *_start;
        }

        ~nfa() {
            std::unordered_set<node_type*> visited;
            std::queue<node_type*> to_visit;

            to_visit.push(_start);
            visited.insert(_start);

            while (!to_visit.empty()) {
                for (auto&& transition : to_visit.front()->transitions) {
                    if (visited.find(transition.next) == visited.end()) {
                        to_visit.push(transition.next);
                        visited.insert(transition.next);
                    }
                }
                delete to_visit.front();
                to_visit.pop();
            }
        }
    };

    template <typename CharType, typename TokenInfo>
    struct dfa_node;

    template <typename CharType, typename TokenInfo>
    struct dfa_transition {
        using node_type = dfa_node<CharType, TokenInfo>;

        boost::icl::interval_set<CharType> characters;
        node_type* next;

        explicit dfa_transition(node_type* next_) : next(next_) {
        }

        dfa_transition(boost::icl::interval_set<CharType> characters_, node_type* next_)
          : characters(characters_), next(next_) {
        }
    };

    template <typename CharType, typename TokenInfo>
    struct dfa_node {
        using nfa_node_type = nfa_node<CharType, TokenInfo>;

        std::unordered_set<const nfa_node_type*> nodes;
        std::vector<dfa_transition<CharType, TokenInfo>> transitions;

        explicit dfa_node(const std::unordered_set<const nfa_node_type*>& nfa_nodes)
          : nodes(nfa_nodes) {
        }

        bool terminal() const {
            for (const auto& node : nodes) {
                if (node->terminal)
                    return true;
            }
            return false;
        }

        TokenInfo get_terminal() const {
            std::vector<TokenInfo> ti;
            for (auto&& nfa_node : nodes) {
                if (nfa_node->terminal)
                    ti.push_back(nfa_node->token);
            }

            if (ti.empty())
                return TokenInfo();
            return *std::min_element(ti.begin(), ti.end());
        }

        dfa_node() {
        }
    };

    /**
     * Returns all states reachable from any of the input NFA states by following
     * a transition containing specified interval (this is a generalization of
     * move(T, a) for alphabet symbol a to intervals of symbols)
     */
    template <typename T, typename CharType, typename TokenInfo>
    void move_s(
        std::unordered_set<const nfa_node<CharType, TokenInfo>*>& dest,
        const T& nodes,
        const boost::icl::discrete_interval<CharType>& chars
    ) {
        for (auto&& node : nodes) {
            for (auto&& trans : node->transitions) {
                if (boost::icl::contains(trans.characters, chars)) {
                    dest.insert(trans.next);
                }
            }
        }
    }

    /**
     * Computes the epsilon-closure of a set of NFA states i.e. all states
     * reachable
     * from them following
     * only epsilon transitions.
     */
    template <typename CharType, typename TokenInfo, typename T>
    void eclosure(std::unordered_set<const nfa_node<CharType, TokenInfo>*>& dest, const T& nodes) {
        std::queue<const nfa_node<CharType, TokenInfo>*> to_visit;

        for (auto&& node : nodes) {
            to_visit.push(node);
            dest.insert(node);
        }

        while (!to_visit.empty()) {
            for (auto&& trans : to_visit.front()->transitions) {
                if (trans.epsilon) {
                    if (dest.find(trans.next) == dest.end()) {
                        to_visit.push(trans.next);
                        dest.insert(trans.next);
                    }
                }
            }
            to_visit.pop();
        }
    }

    /**
     * Takes a collection of interval sets and splits their union into disjoint
     * intervals corresponding to all
     * possible intersections between collections.
     */
    template <typename CharType, typename T>
    void make_disjoint(std::set<boost::icl::discrete_interval<CharType>>& dest, const T& nodes) {
        std::vector<std::pair<bool, CharType>> vec;
        for (auto&& node : nodes) {
            for (auto&& trans : node->transitions) {
                for (auto&& inter : trans.characters) {
                    vec.push_back({ true, inter.lower() });
                    vec.push_back({ false, inter.upper() });
                }
            }
        }
        sort(vec.begin(), vec.end(), [](std::pair<bool, CharType> a, std::pair<bool, CharType> b) {
            return a.second < b.second || ((a.second == b.second) && (a.first && !b.first));
        });

        int depth = 0;
        std::pair<bool, CharType> last;

        for (auto current : vec) {
            if (depth != 0) {
                auto lower = last.second + (last.first ? 0 : 1);
                auto upper = current.second - (current.first ? 1 : 0);

                if (lower <= upper) {
                    dest.insert(boost::icl::construct<boost::icl::discrete_interval<CharType>>(
                        lower, upper, boost::icl::interval_bounds::closed()
                    ));
                }
            }

            last = current;

            if (current.first) {
                depth++;
            }
            if (!current.first) {
                depth--;
            }
        }
    }

    template <typename CharType, typename TokenInfo>
    class dfa {
      public:
        using node_type = dfa_node<CharType, TokenInfo>;
        using nfa_node_type = nfa_node<CharType, TokenInfo>;
        using set_type = std::unordered_set<dfa_node<CharType, TokenInfo>*>;

      private:
        set_type _nodes;
        node_type* _initial;

        dfa(const dfa& other) = delete;
        dfa& operator=(const dfa&) = delete;

        struct dfa_set_hash {
            size_t operator()(const set_type& x) const {
                size_t hash = 2166136261;
                for (auto&& part : x)
                    hash = hash * 16777619 ^ reinterpret_cast<size_t>(part);
                return hash;
            }
        };

        struct dfa_set_eq {
            bool operator()(const set_type& x, const set_type& y) const {
                return x == y;
            }
        };

        struct dfa_node_hash {
            size_t operator()(node_type* x) const {
                size_t hash = 2166136261;
                for (auto&& part : x->nodes)
                    hash = hash * 16777619 ^ reinterpret_cast<size_t>(part);
                return hash;
            }
        };

        struct dfa_node_eq {
            bool operator()(node_type* x, node_type* y) const {
                return x->nodes == y->nodes;
            }
        };

        void aggregate() {
            for (auto& node : _nodes) {
                std::sort(
                    node->transitions.begin(),
                    node->transitions.end(),
                    [](const dfa_transition<CharType, TokenInfo>& a,
                       const dfa_transition<CharType, TokenInfo>& b) { return a.next < b.next; }
                );

                for (size_t i = 1; i < node->transitions.size(); ++i) {
                    if (node->transitions[i].next == node->transitions[i - 1].next) {
                        for (auto&& inter : node->transitions[i].characters)
                            node->transitions[i - 1].characters.add(inter);
                        node->transitions.erase(node->transitions.begin() + i);
                        --i;
                    }
                }
            }
        }

        /**
         * Part of the Moore's algorithm for DFA state minimization. Check whether
         * two
         * states are equivalent under a
         * specified equivalence relation (for each input symbol a, the
         * a-transitions of both states lead to the same element of the set
         * partition.
         */
        bool check_equivalence(
            const node_type* state1,
            const node_type* state2,
            const std::unordered_map<node_type*, const set_type*>& Map
        ) {
            if (state1 == state2)
                return true;
            std::set<boost::icl::discrete_interval<CharType>> letters;
            make_disjoint<CharType>(
                letters, std::initializer_list<const node_type*>{ state1, state2 }
            );

            for (auto&& interval : letters) {
                node_type* next1 = nullptr;
                node_type* next2 = nullptr;

                for (const auto& tran : state1->transitions) {
                    if (boost::icl::contains(tran.characters, interval)) {
                        next1 = tran.next;
                    }
                }

                for (const auto& tran : state2->transitions) {
                    if (boost::icl::contains(tran.characters, interval)) {
                        next2 = tran.next;
                    }
                }

                if ((next1 == nullptr) ^ (next2 == nullptr))
                    return false;
                if ((next1 == nullptr) && (next2 == nullptr))
                    continue;
                if (Map.at(next1) != Map.at(next2))
                    return false;
            }

            return true;
        }

      public:
        dfa(const nfa_node_type& start) {
            auto initial = new node_type();

            std::unordered_set<node_type*> unmarked;
            std::unordered_set<node_type*, dfa_node_hash, dfa_node_eq> result;
            std::unordered_set<const nfa_node_type*> moved;

            std::initializer_list<const nfa_node_type*> start_nodes{ &start };
            eclosure(initial->nodes, start_nodes);

            result.insert(initial);
            unmarked.insert(initial);

            while (!unmarked.empty()) {
                auto it = unmarked.begin();
                auto D = *it;

                unmarked.erase(it);

                std::set<boost::icl::discrete_interval<CharType>> letters;
                make_disjoint<CharType>(letters, D->nodes);

                for (auto&& inter : letters) {
                    auto new_node = new node_type();
                    moved.clear();
                    move_s(moved, D->nodes, inter);
                    eclosure(new_node->nodes, moved);

                    bool inserted = false;
                    if (!new_node->nodes.empty()) {
                        auto status = result.emplace(new_node);
                        if (status.second) {
                            unmarked.insert(new_node);
                            inserted = true;
                        }

                        boost::icl::interval_set<CharType> iset;
                        iset.add(inter);
                        D->transitions.emplace_back(iset, *status.first);
                    }

                    if (!inserted)
                        delete new_node;
                }
            }
            _nodes = set_type(result.begin(), result.end());
            _initial = initial;
            aggregate();
        }

        /**
         * Moore's algorithm for DFA state minimization. Identifies all the
         * indistinguishable subsets of
         * DFA states and replaces them with a single state.
         */
        void optimize() {
            // Compilers: Principles, Techniques and Tools SE, page 182
            std::unordered_set<set_type, dfa_set_hash, dfa_set_eq> Gamma, newGamma;
            std::unordered_map<node_type*, const set_type*> Map, newMap;
            set_type newG;

            // Create the initial partition - to accepting and non-accepting states
            Gamma.insert(_nodes);
            bool initialPartition = true;

            // Refine partitions until there are no more changes.
            while (true) {
                for (auto G : Gamma) // Copy
                {
                    while (!G.empty()) {
                        newG.clear();
                        node_type* pivot = nullptr;

                        for (auto it = G.begin(); it != G.end();) {
                            bool equiv;
                            if (pivot == nullptr)
                                pivot = *it;

                            // Initial partition is made by grouping all the terminal states
                            // emiting the same token type (all non-terminals also together).
                            if (initialPartition)
                                equiv = (pivot->terminal() == (*it)->terminal()) &&
                                        (pivot->get_terminal() == (*it)->get_terminal());
                            else
                                equiv = check_equivalence(pivot, *it, Map);

                            if (equiv) {
                                newG.insert(*it);
                                G.erase(it++);
                            } else {
                                ++it;
                            }
                        }

                        auto subset = &*newGamma.insert(newG).first;

                        for (auto it = newG.begin(); it != newG.end(); ++it) {
                            newMap.emplace(*it, subset);
                        }
                    }
                }

                initialPartition = false;

                if (newGamma.size() == Gamma.size())
                    break;
                else {
                    newGamma.swap(Gamma);
                    newMap.swap(Map);

                    newGamma.clear();
                    newMap.clear();
                }
            }

            set_type dead_states;

            _initial = *newMap.at(_initial)->begin();

            for (auto& node : _nodes) {
                for (auto& transitions : node->transitions) {
                    auto& equiv_class = *newMap.at(transitions.next);
                    auto it = equiv_class.begin();
                    transitions.next = *it;
                    while (++it != equiv_class.end()) {
                        dead_states.insert(*it);
                    }
                }
            }

            for (auto& node : dead_states) {
                _nodes.erase(node);
                delete node;
            }

            aggregate();
        }

        const node_type* start() const {
            return _initial;
        }
        const set_type& states() const {
            return _nodes;
        }

        ~dfa() {
            for (auto node : _nodes)
                delete node;
        }
    };

    template <typename AdditionalInfo>
    struct basic_token_info {
        int precedence;
        AdditionalInfo additional_info;

        friend bool operator<(const basic_token_info& lhs, const basic_token_info& rhs) {
            return (lhs.precedence < rhs.precedence);
        }

        friend bool operator==(const basic_token_info& lhs1, const basic_token_info& rhs1) {
            return lhs1.precedence == rhs1.precedence;
        }

        friend bool operator!=(const basic_token_info& lhs1, const basic_token_info& rhs1) {
            return !(lhs1 == rhs1);
        }
    };

    template <typename CharType, typename AdditionalInfo>
    struct lexer_transition {
        boost::icl::interval_set<CharType> characters;
        size_t next;

        explicit lexer_transition(size_t next_) : next(next_) {
        }
        lexer_transition(boost::icl::interval_set<CharType> characters_, size_t next_)
          : characters(characters_), next(next_) {
        }
    };

    template <typename CharType, typename AdditionalInfo>
    struct lexer_node {
        std::vector<lexer_transition<CharType, AdditionalInfo>> transitions;

        bool terminal;
        AdditionalInfo terminal_info;

        lexer_node(const dfa_node<CharType, basic_token_info<AdditionalInfo>>& node)
          : terminal(node.terminal()), terminal_info(node.get_terminal().additional_info) {
        }
    };

    template <typename CharType, typename AdditionalInfo>
    class lexer {
        using lexer_node_type = lexer_node<CharType, AdditionalInfo>;
        std::vector<lexer_node_type> _states;
        int _start;

      public:
        template <typename Iterator>
        lexer(Iterator begin, Iterator end, int start) : _states(begin, end), _start(start) {
        }

        const std::vector<lexer_node_type>& states() const {
            return _states;
        }

        int start() const {
            return _start;
        }
    };

    template <typename CharType, typename AdditionalInfo>
    struct lexer_production {
        std::basic_string<CharType> regex;
        AdditionalInfo node;

        lexer_production(AdditionalInfo node_, const std::basic_string<CharType>& regex_)
          : regex(regex_), node(node_){};
    };

    template <typename CharType, typename AdditionalInfo>
    class lexer_generator {
        using token_info_type = basic_token_info<AdditionalInfo>;
        using terminal_node_type = terminal_node<CharType, token_info_type>;

        std::vector<terminal_node_type> productions;
        int seq_number;

      public:
        lexer_generator() : seq_number(0){};

        friend lexer_generator& operator<<(
            lexer_generator& lhs,
            const lexer_production<CharType, AdditionalInfo>& terminal
        ) {
            token_info_type info;
            info.precedence = lhs.seq_number++;
            info.additional_info = terminal.node;
            lhs.productions.emplace_back(terminal_node_type(info, terminal.regex));
            return lhs;
        }

        lexer<CharType, AdditionalInfo> generate() {
            nfa<CharType, token_info_type> nfa_machine(productions.begin(), productions.end());
            dfa<CharType, token_info_type> dfa_machine(nfa_machine.start());
            dfa_machine.optimize();

            std::vector<lexer_node<CharType, AdditionalInfo>> nodes;
            std::unordered_map<const dfa_node<CharType, token_info_type>*, int> index;

            for (const auto& state : dfa_machine.states()) {
                index[state] = nodes.size();
                nodes.emplace_back(*state);
            }

            for (const auto& state : dfa_machine.states()) {
                for (const auto& transition : state->transitions) {
                    nodes[index[state]].transitions.emplace_back(
                        transition.characters, index[transition.next]
                    );
                }
            }

            return lexer<CharType, AdditionalInfo>(
                nodes.begin(), nodes.end(), index[dfa_machine.start()]
            );
        }
    };
} // namespace supercomplex
