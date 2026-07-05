#pragma once
#include <vector>
#include <map>
#include <set>
#include <queue>
#include "Regex.cpp"
#include "automata.cpp"



struct State {
    bool is_final = false;
    std::string token_type;
    std::map<char, std::vector<State*>> transitions;
    std::vector<State*> epsilon_transitions;
};


struct NFA {
    State* start;
    State* end;
    string lexeme;
};


NFA ast_to_nfa (const Node& raw_node) {
    State* start = new State();
    State* end = new State();
    string lexeme;

    if (const SymbolNode* node = dynamic_cast<const SymbolNode*>(&raw_node)){
        end->is_final = true;
        start->transitions[node->value].push_back(end);
        lexeme = string(1, node->value);
    }
    else if (const ConcatNode* node = dynamic_cast<const ConcatNode*>(&raw_node)){
        NFA left = ast_to_nfa(*node->left);
        NFA right = ast_to_nfa(*node->right);

        left.end->epsilon_transitions.push_back(right.start); // Une los NFAs

        left.end->is_final = false;
        return {left.start, right.end, left.lexeme + right.lexeme};  // Usa los starts/ends de los sub-NFAs
    }
    else if (const UnionNode* node = dynamic_cast<const UnionNode*>(&raw_node)){
        end->is_final = true;

        NFA left = ast_to_nfa(*node->left);
        NFA right = ast_to_nfa(*node->right);

        start->epsilon_transitions.push_back(left.start);
        start->epsilon_transitions.push_back(right.start);
        left.end->epsilon_transitions.push_back(end);
        right.end->epsilon_transitions.push_back(end);
        left.end->is_final = right.end->is_final = false;
        lexeme = "(" + left.lexeme + "|" + right.lexeme + ")";
    }
    else if (const ClosureNode* node = dynamic_cast<const ClosureNode*>(&raw_node)){
        end->is_final = true;

        NFA child = ast_to_nfa(*node->child);

        start->epsilon_transitions.push_back(child.start);
        start->epsilon_transitions.push_back(end);
        child.end->epsilon_transitions.push_back(child.start);
        child.end->epsilon_transitions.push_back(end);
        child.end->is_final = false;
        lexeme = "(" + child.lexeme + ")*";
    }
    else if (const GroupNode* node = dynamic_cast<const GroupNode*>(&raw_node)){
        NFA body = ast_to_nfa(*node->expr);
        body.lexeme = "(" + body.lexeme + ")";
        return body;
    }
    else {
        cout << raw_node.getType() << endl;
        throw runtime_error("Tipo de nodo no soportado en ast_to_nfa");
    }

    return {start, end, lexeme};
}

NFA combine_nfas(const vector<NFA>& nfas) {
    State* start = new State();
    for (const NFA& nfa : nfas) {
        start->epsilon_transitions.push_back(nfa.start);
    }
    return {start, nullptr, ""}; // El estado final se determina al tokenizar
}

    // Agregar al final de automata.cpp

AFND nfa_to_afnd(const NFA& nfa) {
    if (nfa.start == nullptr) {
        // NFA vacío: crear AFND con un estado no final
        unordered_set<int> final_states;
        unordered_map<pair<int, char>, vector<int>, AFND::Pair_Hash> transitions;
        return AFND(1, final_states, transitions, 0);
    }

    unordered_map<State*, int> state_to_id;
    queue<State*> q;
    int next_id = 0;

    // Asignar ID al estado inicial
    q.push(nfa.start);
    state_to_id[nfa.start] = next_id++;

    // BFS para recorrer todos los estados
    while (!q.empty()) {
        State* current = q.front();
        q.pop();

        // Procesar transiciones por símbolo
        for (const auto& transition : current->transitions) {
            char symbol = transition.first;
            for (State* next_state : transition.second) {
                if (state_to_id.find(next_state) == state_to_id.end()) {
                    state_to_id[next_state] = next_id++;
                    q.push(next_state);
                }
            }
        }

        // Procesar transiciones épsilon
        for (State* next_state : current->epsilon_transitions) {
            if (state_to_id.find(next_state) == state_to_id.end()) {
                state_to_id[next_state] = next_id++;
                q.push(next_state);
            }
        }
    }

    // Construir componentes del AFND
    unordered_map<pair<int, char>, vector<int>, AFND::Pair_Hash> transitions;
    unordered_set<int> final_states;

    for (const auto& entry : state_to_id) {
        State* state_ptr = entry.first;
        int id = entry.second;

        // Registrar estado final
        if (state_ptr->is_final && state_ptr != nullptr) {
            final_states.insert(id);
        }

        // Registrar transiciones por símbolo
        for (const auto& transition : state_ptr->transitions) {
            char symbol = transition.first;
            for (State* next_state : transition.second) {
                int next_id_val = state_to_id[next_state];
                transitions[{id, symbol}].push_back(next_id_val);
            }
        }

        // Registrar transiciones épsilon (símbolo '\0')
        for (State* next_state : state_ptr->epsilon_transitions) {
            int next_id_val = state_to_id[next_state];
            transitions[{id, '\0'}].push_back(next_id_val);
        }
    }

    int n_states = next_id;
    int start_state = state_to_id[nfa.start];

    auto afnd = AFND(n_states, final_states, transitions, start_state);
//    afnd.print();
//    cout << "\n\n";

    return afnd;
}

AFD afnd_to_afd(AFND grafo_origen)
{
    NFA_Converter converter;
    auto grafo_destino = converter.convertToAFD(grafo_origen);
    //grafo_destino.print();

    return grafo_destino;
}

AFD Convert(const Node& ast)
{
    auto nfa = ast_to_nfa(ast);
    auto afnd = nfa_to_afnd(nfa);
    auto afd = afnd_to_afd(afnd);

    return afd;
}
