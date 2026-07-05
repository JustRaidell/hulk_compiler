#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdexcept>
#include <queue>
#include <utility>
#include <set>
#include <deque>

using namespace std;


class AFND {
    public:
        struct Pair_Hash {
            std::size_t operator()(const pair<int,char>& p) const {
                return hash<int>()(p.first) ^ (hash<char>()(p.second) << 1);
            }
        };

    private:
        int n_states;
        unordered_set<int> final_states;
        unordered_map<pair<int,char>, vector<int>, Pair_Hash> transitions;
        int start_state;


    public:
        /**
         * Constructor del autómata finito no determinista
         * @param n_states Número de estados del autómata
         * @param final_states Conjunto de estados finales
         * @param transitions Diccionario de transiciones del autómata
         * @param start_state Estado inicial del autómata
         */
        AFND(int n_states,
            const unordered_set<int>& final_states,
            const unordered_map<pair<int,char>, vector<int>, Pair_Hash>& transitions,
            int start_state = 0):
                n_states(n_states),
                start_state(start_state)
                {

                    for (int s : final_states) {
                        if (s >= n_states) throw invalid_argument("Estado final fuera de rango");
                        this->final_states.insert(s);
                    }
                    this->transitions = transitions;

                    validate();
                }
        AFND(int n_states,
            const unordered_set<int>& final_states,
            int start = 0):
                n_states(n_states),
                start_state(start)
                {

                    for (int s : final_states) {
                        if (s >= n_states) throw invalid_argument("Estado final fuera de rango");
                        this->final_states.insert(s);
                    }
                    validate();
                }

    private:
        void validate() {
            for (int state: final_states) {
                if (state < 0 || state >= n_states) {
                    throw invalid_argument(
                        "Estado final fuera de rango: " + to_string(state) +
                        " [0 a " + to_string(n_states - 1) + "]"
                    );

                }
            }
            if (start_state < 0 || start_state >= n_states) {
                throw invalid_argument(
                    "Estado inicial fuera de rango: " + to_string(start_state) +
                    " [0 a " + to_string(n_states - 1) + "]"
                );
            }
            for (const auto& [key,dests] : transitions) {
                int state = key.first;

                if (state < 0 || state >= n_states) {
                    throw invalid_argument(
                        "Estado de transicion fuera de rango: " + to_string(state) +
                        " [0 a " + to_string(n_states - 1) + "]"
                    );
                }
                for (int dest : dests) {
                    if (dest < 0 || dest >= n_states) {
                        throw invalid_argument(
                            "Destino de transicion fuera de rango: " + to_string(dest) +
                            " [0 a " + to_string(n_states - 1) + "]"
                        );
                    }
                }
            }
        }

    public:
        void addTransition(int from, char symbol, int to) {
            if (from < 0 || from >= n_states || to < 0 || to >= n_states) throw invalid_argument("Estados fuera del rango valido");
            transitions[{from,symbol}].push_back(to);
        }
        vector<int> getTransitions(int state, char symbol) const {
            auto it = transitions.find({state, symbol});
            if (it != transitions.end()) {
                return it->second;
            }
            return vector<int>();
        }

        /*
         * Calculo de epsilon-clausura de un conjunto de estados
         * util si el automata tiene transiciones epsilon
        */
        unordered_set<int> epsilonClosure(const unordered_set<int>& states) const {
            unordered_set<int> closure = states;
            queue<int> _queue;

            for (int state : states) {
                _queue.push(state);
            }

            while (!_queue.empty()) {
                int current = _queue.front();
                _queue.pop();

                vector<int> tmpTransitions = getTransitions(current, '\0'); // Asumiendo que '\0' representa una transición epsilon
                for (int nextState : tmpTransitions) {
                    if (closure.find(nextState) == closure.end()) {
                        closure.insert(nextState);
                        _queue.push(nextState);
                    }
                }
            }
            return closure;
        }

        bool recognize(const string& word) const {
            unordered_set<int> currentStates = epsilonClosure({start_state});

            for (char symbol : word) {
                unordered_set<int> nextStates;

                // Para cada estado actual, encontrar todos los estados alcanzables
                for (int state : currentStates) {
                    vector<int> transitions = getTransitions(state, symbol);
                    for (int nextState : transitions) {
                        nextStates.insert(nextState);
                    }
                }

                currentStates = epsilonClosure(nextStates);
                if (currentStates.empty()) {
                    return false; // No hay estados alcanzables
                }
            }

            for (int state : currentStates) {
                if (final_states.find(state) != final_states.end()) {
                    return true; // Al menos un estado final alcanzado
                }
            }
            return false;
        }

        void print() const {
            cout << "AFND con " << n_states << " estados." << endl;
            cout << "Estado inicial: " << start_state << endl;
            cout << "Estados finales: ";
            for (int state : final_states) {
                cout << state << " ";
            }
            cout << endl;

            cout << "Transiciones:" << endl;
            for (const auto& [key, dests] : transitions) {
                cout << "  Desde estado " << key.first << " con simbolo '"
                     << key.second << "' a estados: ";
                for (int dest : dests) {
                    cout << dest << " ";
                }
                cout << endl;
            }
        }

        bool isFinalState(int state) const { return final_states.find(state) != final_states.end(); }

        // Getters
        int getNumStates() const { return n_states; }
        int getStartState() const { return start_state; }
        const unordered_set<int>& getFinalStates() const { return final_states; }
        const unordered_map<pair<int,char>, vector<int>, Pair_Hash>& getTransitions() const { return transitions; }
};

class AFD : public AFND {
    public:
        AFD(int n_states,
            const unordered_set<int>& final_states,
            const unordered_map<pair<int,char>, vector<int>, AFND::Pair_Hash>& transitions,
            int start_state = 0):
                AFND(n_states, final_states, transitions, start_state) {
                    validateDeterminism();
            }
        AFD(int n_states,
            const unordered_set<int>& final_states,
            int start = 0):
                AFND(n_states, final_states, start) {
                    validateDeterminism();
                }

    private:
        void validateDeterminism() {
            for (const auto& [key, dests] : getTransitions()) {
                if (dests.size() != 1) {
                    throw invalid_argument("El AFD debe tener una unica transicion por simbolo desde cada estado.");
                }
            }
        }

        void addTransition(int from, char symbol, int to) {
            if (from < 0 || from >= getNumStates() || to < 0 || to >= getNumStates()) {
                throw invalid_argument("Estados fuera del rango valido");
            }
            if (getTransitions().find({from, symbol}) != getTransitions().end()) {
                throw invalid_argument("Ya existe una transicion para el estado " + to_string(from) + " con el simbolo '" + symbol + "'");
            }
            AFND::addTransition(from, symbol, to);
        }


    public:
        bool recognize(const string& word) const {
            return match_length(word) == word.length();
        }

        //Devuelve la longitud del prefijo más largo reconocido
        int match_length(const string& text, int pos = 0) const {   //Devuelve size_t en vez de int?
            int current_state = getStartState();
            int last_final_pos = -1;

            for (size_t i = pos; i < text.length(); ++i) {
                char symbol = text[i];
                vector<int> next_states = getTransitions(current_state, symbol);

                if (next_states.empty()) {
                    break;  // No más transiciones posibles
                }

                current_state = next_states[0];  // AFD es determinista

                if (isFinalState(current_state)) {
                    last_final_pos = i + 1 - pos;  // Guardamos posición del match (longitud)
                }
            }

            return last_final_pos;  // -1 si no hubo match, o la longitud del más largo
        }

        //Devuelve el substring reconocido
        string match(const string& text, int pos = 0) const {
            int len = match_length(text, pos);
            return (len > 0) ? text.substr(pos, len) : "";
        }

        void print() const {
            cout << "AFD con " << getNumStates() << " estados." << endl;
            cout << "Estado inicial: " << getStartState() << endl;
            cout << "Estados finales: ";
            for (int state : getFinalStates()) {
                cout << state << " ";
            }
            cout << endl;

            cout << "Transiciones:" << endl;
            for (const auto& [key, dests] : getTransitions()) {
                cout << "  Desde estado " << key.first << " con simbolo '"
                     << key.second << "' a estado: ";
                if (!dests.empty()) {
                    cout << dests[0]; // AFD tiene una unica transicion por simbolo
                } else {
                    cout << "Ninguno";
                }
                cout << endl;
            }
        }

};

class NFA_Converter {
public:
    using StateSet = unordered_set<int>; // Conjunto de estados del AFND


    AFD convertToAFD(const AFND& nfa) {
        using StateSet = unordered_set<int>;

        // Paso 1: ε-clausura del estado inicial
        StateSet initial = nfa.epsilonClosure({nfa.getStartState()});

        // Paso 2: Inicializar AFD
        deque<AFD_State> afd_states;
        unordered_map<StateSet, int, StateSetHash> state_to_id;
        queue<int> to_process;

        // Crear estado inicial
        afd_states.push_back({initial, false, {}});
        state_to_id[initial] = 0;
        to_process.push(0);

        while (!to_process.empty()) {
            int current_id = to_process.front();
            to_process.pop();
            AFD_State& current = afd_states[current_id];

            for (char symbol : getAlphabet(nfa)) {
                StateSet next = computeNextState(nfa, current.subset, symbol);
                if (next.empty()) continue;

                if (!state_to_id.count(next)) {
                    int new_id = afd_states.size();
                    afd_states.push_back({next, containsFinalState(nfa, next), {}});
                    state_to_id[next] = new_id;
                    to_process.push(new_id);
                }

                current.transitions[symbol] = state_to_id[next];
            }
        }

        // Paso 4: Construir el AFD
        return buildAFDFromStates(afd_states);
    }

private:
    struct StateSetHash {
        std::size_t operator()(const std::unordered_set<int>& s) const {
            std::size_t hash = 0;
            for (int elem : s) {
                hash ^= std::hash<int>{}(elem) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            }
            return hash;
        }
    };

    struct AFD_State {
        StateSet subset;  // Subconjunto de estados del AFND
        bool is_final = false;
        unordered_map<char, int> transitions; // Transiciones del AFD (símbolo -> estado)
    };

    set<char> getAlphabet(const AFND& nfa) {
        set<char> alphabet;
        for (const auto& [key, _] : nfa.getTransitions()) {
            if (key.second != '\0') alphabet.insert(key.second); // Ignorar ε
        }
        return alphabet;
    }

    StateSet computeNextState(const AFND& nfa, const StateSet& states, char symbol) {
        StateSet next;
        if (states.empty()) return {};  // Manejar conjunto vacío

        for (int state : states) {
            auto transitions = nfa.getTransitions(state, symbol);
            for (int dest : transitions) {
                next.insert(dest);
            }
        }
        return nfa.epsilonClosure(next); // Incluir ε-clausura
    }

    bool containsFinalState(const AFND& nfa, const StateSet& states) {
        for (int state : states) {
            if (nfa.isFinalState(state)) return true;
        }
        return false;
    }

    AFD buildAFDFromStates(const deque<AFD_State>& afd_states) {
        // 1. Mapear estados del AFD a IDs numéricos (0, 1, 2, ...)
        unordered_map<StateSet, int, StateSetHash> state_ids;
        for (size_t i = 0; i < afd_states.size(); ++i) {
            state_ids[afd_states[i].subset] = i;
        }

        // 2. Construir transiciones del AFD
        unordered_map<pair<int, char>, vector<int>, AFND::Pair_Hash> transitions;
        for (size_t i = 0; i < afd_states.size(); ++i) {
            const AFD_State& state = afd_states[i];
            for (const auto& [symbol, target_id] : state.transitions) {
                transitions[{i, symbol}].push_back(target_id);
            }
        }

        // 3. Identificar estados finales
        unordered_set<int> final_states;
        for (size_t i = 0; i < afd_states.size(); ++i) {
            if (afd_states[i].is_final) {
                final_states.insert(i);
            }
        }

        return AFD(
            afd_states.size(),   // Número de estados
            final_states,        // Estados finales
            transitions,         // Transiciones
            0                   // Estado inicial (S0)
        );
    }
};
