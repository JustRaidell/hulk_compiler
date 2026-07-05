#include <iostream>
#include <iomanip>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <stack>
#include <stdexcept>




#include "grammar.cpp"

using namespace std;

/**
 * Calcula First para una sentencia alpha
 */

// BUG CONOCIDO: computeLocalFirst puede devolver epsilon prematuramente si un no-terminal anulable va seguido de un terminal.
// Actualmente no afecta porque la gramática evita esa situación mediante encapsulación (TypeSuffix, etc.).
// Si en el futuro se añaden más reglas con opcionales consecutivos, revisar. Adjunto código alternativo sin testear que "soluciona" el problema

ContainerSet computeLocalFirst(const unordered_map<Symbol,
                               ContainerSet>& firsts,
                               const Sentence& alpha) {
    ContainerSet first_alpha;

    if (alpha.isEpsilon()) {
        first_alpha.setEpsilon();
    } else {
        for (const Symbol& symbol : alpha) {
            auto it = firsts.find(symbol);
            if (it != firsts.end()) {

                //Código original
                first_alpha.update(it->second);

                //Código alternativo
//                for (const Symbol& s : it->second.getSymbols()) {
//                    first_alpha.insert(s);
//                }

                if (!it->second.containsEpsilon()) {
                    break;
                }
            } else {
                // Es un terminal: se añade directamente y se rompe
                first_alpha.insert(symbol);
                break;
            }
        }

        // Si todos los símbolos pueden derivar epsilon, añadir epsilon
        bool all_can_derive_epsilon = true;

        for (const Symbol& symbol : alpha) {
            auto it = firsts.find(symbol);
            if (it == firsts.end() || !it->second.containsEpsilon()) {
                all_can_derive_epsilon = false;
                break;
            }
        }


        if (all_can_derive_epsilon) {
            first_alpha.setEpsilon();
        }
    }

    return first_alpha;
}

/**
 * Algoritmo principal para calcular First(G) donde G es la Gramatica
 */
unordered_map<Symbol, ContainerSet> computeFirsts(const Grammar& G) {
    unordered_map<Symbol, ContainerSet> firsts;
    unordered_map<Sentence, ContainerSet> sentence_firsts;

    bool change = true;

    // Inicializar First(Vt) - cada terminal tiene como First a si mismo
    for (const Symbol& terminal : G.getTerminals()) {
        firsts[terminal] = ContainerSet(terminal);
    }

    // Inicializar First(Vn) - cada no terminal empieza con conjunto vacio
    for (const Symbol& nonTerminal : G.getNonTerminals()) {
        firsts[nonTerminal] = ContainerSet();
    }

    while (change) {
        change = false;

        // Para cada producción X -> alpha
        for (const Production& production : G.getProductions()) {
            const Symbol& X = production.getLeft();
            const Sentence& alpha = production.getRight();

            // Obtener First(X) actual
            ContainerSet& first_X = firsts[X];

            // Inicializar First(alpha) si no existe
            if (sentence_firsts.find(alpha) == sentence_firsts.end()) {
                sentence_firsts[alpha] = ContainerSet();
            }
            ContainerSet& first_alpha = sentence_firsts[alpha];

            // Calcular First local de alpha
            ContainerSet local_first = computeLocalFirst(firsts, alpha);

            // Actualizar First(alpha) y First(X)
            change |= first_alpha.hardUpdate(local_first);
            change |= first_X.hardUpdate(local_first);
        }
    }

    return firsts;
}


/**
 * Algoritmo principal para calcular Follow(G)
 * Basado en las reglas:
 * 1. Follow(S) contiene #
 * 2. Para B -> αAβ: Follow(A) ∪= (First(β) - {ε})
 * 3. Para B -> αA o B -> αAβ donde ε ∈ First(β): Follow(A) ∪= Follow(B)
 */
unordered_map<Symbol, ContainerSet> computeFollows(const Grammar& G,
                                                   const unordered_map<Symbol, ContainerSet>& firsts) {
    unordered_map<Symbol, ContainerSet> follows;
    bool change = true;

    // Simbolo EOF (final de fichero)
    Symbol EOF_SYMBOL("#", true);

    // Inicializar Follow(Vn) - cada no terminal empieza con conjunto vacio
    for (const Symbol& nonTerminal : G.getNonTerminals()) {
        follows[nonTerminal] = ContainerSet();
    }

    // Follow(S) contiene # (simbolo de inicio)
    follows[G.getStartSymbol()].insert(EOF_SYMBOL);


    while (change) {
        change = false;

        // Para cada producción X -> alpha
        for (const Production& production : G.getProductions()) {
            const Symbol& X = production.getLeft();
            const Sentence& alpha = production.getRight();

            // Si la producción es epsilon, continuar
            if (alpha.isEpsilon()) {
                continue;
            }

            ContainerSet& follow_X = follows[X];
            size_t n = alpha.size();

            // Recorrer todos los símbolos de la parte derecha
            for (size_t i = 0; i < n; ++i) {
                const Symbol& Y = alpha[i];

                // Solo procesamos no terminales
                if (!Y.isNonTerminal()) {
                    continue;
                }

                ContainerSet& follow_Y = follows[Y];

                // Caso: X -> ζ Y β (hay símbolos después de Y)
                if (i < n - 1) {
                    // Crear la cadena β (símbolos después de Y)
                    vector<Symbol> beta_symbols;
                    for (size_t j = i + 1; j < n; ++j) {
                        beta_symbols.push_back(alpha[j]);
                    }
                    Sentence beta(beta_symbols);

                    // Calcular First(β)
                    ContainerSet first_beta = computeLocalFirst(firsts, beta);

                    // Regla S1: Follow(Y) ∪= (First(β) - {ε})
                    ContainerSet first_beta_no_epsilon = first_beta - unordered_set<std::string>{EPSILON, "epsilon"};
                    change |= follow_Y.hardUpdate(first_beta_no_epsilon);

                    // Regla S2: Si ε ∈ First(β), entonces Follow(Y) ∪= Follow(X)
                    if (first_beta.containsEpsilon()) {
                        change |= follow_Y.hardUpdate(follow_X);
                    }
                }
                // Caso: X -> ζ Y (Y es el ultimo simbolo)
                else {
                    // Regla S2: Follow(Y) ∪= Follow(X)
                    change |= follow_Y.hardUpdate(follow_X);
                }
            }
        }
    }

    return follows;
}

/**
 * Funcion auxiliar para imprimir conjuntos First y Follow
 */
void printFirstAndFollow(const Grammar& G,
                        const unordered_map<Symbol, ContainerSet>& firsts,
                        const unordered_map<Symbol, ContainerSet>& follows) {
    cout << "\n=== CONJUNTOS FIRST Y FOLLOW ===\n";

    auto print_set = [](const ContainerSet& cs) {
        cout << "{ ";
        for (const auto& symbol : cs.getSymbols()) {
            if (symbol.getName() == EPSILON) {
                cout << "epsilon ";
            } else {
                cout << symbol.getName() << " ";
            }
        }
        if (cs.containsEpsilon()) {
            cout << "epsilon ";
        }
        cout << "}";
    };

    cout << "Elementos de FIRST:\n";
    for (const auto& pair : firsts) {
        cout << "First(" << pair.first.getName() << ") = ";
        print_set(pair.second);
        cout << "\n";
    }

    cout << "\nElementos de FOLLOW:\n";
    for (const auto& pair : follows) {
        cout << "Follow(" << pair.first.getName() << ") = ";
        print_set(pair.second);
        cout << "\n";
    }

    // print First
    cout << "\nConjuntos FIRST:\n";
    for (const Symbol& nonTerminal : G.getNonTerminals()) {
        auto it = firsts.find(nonTerminal);
        if (it != firsts.end()) {
            cout << "First(" << nonTerminal.getName() << ") = ";
            print_set(it->second);
            cout << "\n";
        }
    }

    // print Follow
    cout << "\nConjuntos FOLLOW:\n";
    for (const Symbol& nonTerminal : G.getNonTerminals()) {
        auto it = follows.find(nonTerminal);
        if (it != follows.end()) {
            cout << "Follow(" << nonTerminal.getName() << ") = ";
            print_set(it->second);
            cout << "\n";
        }
    }
}



class ParsingTable {
public:
    struct ParsingLL1_Pair_Hash {
        size_t operator()(const pair<Symbol,Symbol>& p) const {
            return hash<string>()(p.first.getName()) ^ (hash<string>()(p.second.getName()) << 1);
        }
    };

    // Diccionario: key (no terminal, terminal), value: producción
    unordered_map<pair<Symbol, Symbol>, Production, ParsingLL1_Pair_Hash> TABLE;
    Grammar G;

    ParsingTable(const Grammar& G) : G(G) {
        buildTable();
    }

private:
    void buildTable() {
        auto first = computeFirsts(G);
        auto follow = computeFollows(G, first);


    }

};




/**
 * Clase que implementa el parser LL(1)
 */
class LL1Parser {
private:
    Grammar G;
    // Tabla de parsing: TABLE[NonTerminal][Terminal] -> Production
    unordered_map<Symbol, unordered_map<Symbol, Production>> TABLE;
    unordered_map<Symbol, ContainerSet> firsts;
    unordered_map<Symbol, ContainerSet> follows;
    Symbol EOF_SYMBOL;

    /**
     * Construye la tabla de análisis LL(1)
     */
    void buildParsingTable() {
        // Calcular conjuntos First y Follow
        firsts = computeFirsts(G);
        follows = computeFollows(G, firsts);


        // Inicializar tabla vacia
        TABLE.clear();

        // Para cada produccion A -> α
        for (const Production& production : G.getProductions()) {
             const Symbol& A = production.getLeft();
             const Sentence& alpha = production.getRight();

            // Calcular conjunto de prediccion para la produccion
            ContainerSet prediction_set = computePredictionSet(production);

            // Para cada terminal a en Pred(A -> α)
            for (const Symbol& terminal : prediction_set.getSymbols()) {
                // Verificar si ya existe una entrada en M[A, a]
                if (TABLE[A].find(terminal) != TABLE[A].end()) {
//                        cout << TABLE[A][terminal].toString() << endl;
//                        cout << production.toString() << endl;
                    throw runtime_error("La gramatica no es LL(1): conflicto en TABLE[" +
                                        A.getName() + ", " + terminal.getName() + "]");
                }
                TABLE[A][terminal] = production;
            }

            // Si epsilon está en el conjunto de prediccion
            if (prediction_set.containsEpsilon()) {
                // Para cada simbolo en Follow(A)
                const ContainerSet& follow_A = follows[A];
                for (const Symbol& follow_symbol : follow_A.getSymbols()) {
                    if (TABLE[A][follow_symbol] == production) continue;
                    if (TABLE[A].find(follow_symbol) != TABLE[A].end()) {
//                            cout << TABLE[A][terminal].toString() << endl;
//                            cout << production.toString() << endl;
                        throw runtime_error("La gramatica no es LL(1): conflicto en TABLE[" +
                                            A.getName() + ", " + follow_symbol.getName() + "]");
                    }
                    TABLE[A][follow_symbol] = production;
                }
            }
        }
    }

    /**
     * Calcula el conjunto de prediccion para una produccion A -> α
     */
    ContainerSet computePredictionSet(const Production& production) {
        const Symbol& A = production.getLeft();
        const Sentence& alpha = production.getRight();
        ContainerSet prediction_set;

        if (alpha.isEpsilon()) {
            // Si α = ε, entonces Pred(A -> α) = Follow(A)
            prediction_set.update(follows[A]);
        } else {
            // Pred(A -> α) = First(α)
            ContainerSet first_alpha = computeLocalFirst(firsts, alpha);
            prediction_set.update(first_alpha);

            // Si ε ∈ First(α), entonces Pred(A -> α) = First(α) ∪ Follow(A)
            if (first_alpha.containsEpsilon()) {
                prediction_set.update(follows[A]);
            }
        }

        return prediction_set;
    }

public:
    /**
     * Constructor del parser LL(1)
     */
    LL1Parser(const Grammar& grammar)
        : G(grammar), EOF_SYMBOL("#", true) {
        buildParsingTable();
    }

    /**
     * Realiza el análisis sintáctico de una cadena de entrada
     * @param input Cadena de entrada terminada en EOF (#)
     * @return Vector de producciones aplicadas en orden
     */
    vector<Production> parse(const vector<Symbol>& input) {
        vector<Production> output;
        stack<Symbol> parsing_stack;
        size_t cursor = 0;

        // Inicializar pila con EOF y símbolo inicial
        parsing_stack.push(EOF_SYMBOL);
        parsing_stack.push(G.getStartSymbol());

        while (!parsing_stack.empty()) {
            Symbol top = parsing_stack.top();
            parsing_stack.pop();

            // Verificar bounds del cursor
            if (cursor >= input.size()) {
                throw runtime_error("Entrada insuficiente durante el analisis");
            }

            Symbol current_input = input[cursor];

            if (top.getName() == EPSILON || top.getName() == "epsilon") {
                // Símbolo epsilon - no hacer nada
                continue;
            }
            else if (top.isTerminal()) {
                // Top es terminal
                if (top == current_input) {
                    if (top == EOF_SYMBOL) {
                        // Análisis exitoso
                        break;
                    }
                    cursor++;
                } else {
                    throw runtime_error("Error sintactico: esperado '" + top.getName() +
                                      "', encontrado '" + current_input.getName() + "'");
                }
            }
            else {
                // Top es no terminal
                auto it_A = TABLE.find(top);
                if (it_A == TABLE.end()) {
                    throw runtime_error("No terminal no encontrado en tabla: " + top.getName());
                }

                auto it_prod = it_A->second.find(current_input);
                if (it_prod == it_A->second.end()) {
                    throw runtime_error("Error sintactico: no hay entrada en TABLE[" +
                                      top.getName() + ", " + current_input.getName() + "]");
                }

                Production production = it_prod->second;
                output.push_back(production);

                // Expandir producción en la pila (en orden inverso)
                const Sentence& right_side = production.getRight();
                if (!right_side.isEpsilon()) {
                    // Apilar símbolos en orden inverso
                    for (int i = right_side.size() - 1; i >= 0; i--) {
                        parsing_stack.push(right_side[i]);
                    }
                }
            }
        }

        return output;
    }

    /**
     * Verifica si la gramática es LL(1)
     */
    bool isLL1() const {
        try {
            // Si la construcción de la tabla fue exitosa, es LL(1)
            return true;
        } catch (const runtime_error&) {
            return false;
        }
    }

    /**
     * Imprime la tabla de análisis LL(1)
     */
    void printParsingTable() const {
        cout << "\n=== TABLA DE ANALISIS LL(1) ===\n";

        // Obtener todos los terminales para las columnas
        unordered_set<Symbol> all_terminals = G.getTerminals();
        all_terminals.insert(EOF_SYMBOL);

        // Imprimir encabezados
        cout << setw(12) << "TABLE[A,a]";
        for (const Symbol& terminal : all_terminals) {
            cout << setw(15) << terminal.getName();
        }
        cout << "\n";

        // Imprimir filas para cada no terminal
        for (const Symbol& nonTerminal : G.getNonTerminals()) {
            cout << setw(12) << nonTerminal.getName();

            for (const Symbol& terminal : all_terminals) {
                auto it_A = TABLE.find(nonTerminal);
                if (it_A != TABLE.end()) {
                    auto it_prod = it_A->second.find(terminal);
                    if (it_prod != it_A->second.end()) {
                        string prod_str = it_prod->second.toString();
                        if (prod_str.length() > 14) {
                            prod_str = prod_str.substr(0, 11) + "...";
                        }
                        cout << setw(15) << prod_str;
                    } else {
                        cout << setw(15) << "ERROR";
                    }
                } else {
                    cout << setw(15) << "ERROR";
                }
            }
            cout << "\n";
        }
    }


    /**
     * Imprime el resultado del análisis
     */
    void printParseResult(const vector<Production>& derivations) const {
        cout << "\n=== DERIVACIONES APLICADAS ===\n";
        for (size_t i = 0; i < derivations.size(); i++) {
            cout << (i + 1) << ". " << derivations[i].toString() << "\n";
        }
    }
};




///Definiendo la gramática de HULK

Grammar BuildHulkGrammar(){

    unordered_set<Symbol> terminals = {
        Symbol("+", true), Symbol("-", true), Symbol("*", true), Symbol("/", true), Symbol("%", true),
        Symbol("(", true), Symbol(")", true), Symbol("[", true), Symbol("]", true),
        Symbol("{", true), Symbol("}", true), Symbol(";", true),
        Symbol("function", true), Symbol("=>", true), Symbol(",", true),
        Symbol("let", true), Symbol("=", true), Symbol("in", true),
        Symbol("if", true), Symbol("elif", true), Symbol("else", true),
        Symbol("while", true), Symbol("for", true), Symbol("range", true),
        Symbol("type", true), Symbol("new", true), Symbol(".", true), Symbol("inherits", true),
        Symbol(":", true), Symbol("Number", true), Symbol("String", true), Symbol("Boolean", true), Symbol("is", true),
        Symbol("protocol", true), Symbol("extends", true), Symbol("->", true),
        Symbol("def", true),
        Symbol("@", true), Symbol("@@", true), Symbol("$", true),
        Symbol("==", true), Symbol("!=", true), Symbol("<", true), Symbol("<=", true), Symbol(">", true), Symbol(">=", true),
        Symbol("!", true), Symbol("|", true), Symbol("&", true), Symbol("true", true), Symbol("false", true),
        Symbol("id", true), Symbol("number", true), Symbol("string", true), Symbol("\"", true),
        Symbol("#", true)
    };

    unordered_set<Symbol> nonTerminals = {
        Symbol("Program", false), Symbol("Body", false), Symbol("Tail", false),
        Symbol("Function", false), Symbol("Params", false),
        Symbol("Expression", false), Symbol("ExprBlock", false),
        Symbol("Vector", false), Symbol("VecInit", false),
        Symbol("AssignExpr", false), Symbol("Assignation", false), Symbol("Reassignment", false),
        Symbol("CondExpr", false), Symbol("Condition", false),
        Symbol("LoopExpr", false), Symbol("WhileExpr", false), Symbol("ForExpr", false), Symbol("Collection", false),
        Symbol("TypeDef", false), Symbol("TypeParams", false), Symbol("Inheritance", false),
        Symbol("ProtocolDef", false), Symbol("Prot", false), Symbol("Extension", false),
        Symbol("Expr", false)
    };

    vector<Production> productions = {

        //Nivel superior
        Production(Symbol("Program", false), Sentence({Symbol("ProtocolList", false), Symbol("TypeList", false), Symbol("FuncList", false), Symbol("Body", false), Symbol(";", true)})),


        //Tipos de Expresiones
        Production(Symbol("Expression", false), Sentence({Symbol("Expr", false)})),
        Production(Symbol("Expression", false), Sentence({Symbol("AssignExpr", false)})),
        Production(Symbol("Expression", false), Sentence({Symbol("Reassignment", false)})),
        Production(Symbol("Expression", false), Sentence({Symbol("CondExpr", false)})),
        Production(Symbol("Expression", false), Sentence({Symbol("LoopExpr", false)})),


        Production(Symbol("Body", false), Sentence({Symbol("Expression", false)})),
        Production(Symbol("Body", false), Sentence({Symbol("ExprBlock", false)})),

        Production(Symbol("Tail", false), Sentence({Symbol(";", true)})),
        Production(Symbol("Tail", false), Sentence()),


        Production(Symbol("ExprBlock", false), Sentence({Symbol("{", true), Symbol("ExprList", false), Symbol("}", true)})),

        Production(Symbol("ExprList", false), Sentence({Symbol("Expression", false), Symbol(";", true), Symbol("ExprList", false)})),
        Production(Symbol("ExprList", false), Sentence()),




        //Gramática de protocolos
        Production(Symbol("ProtocolList", false), Sentence({Symbol("ProtocolDef", false), Symbol("ProtocolList", false)})),
        Production(Symbol("ProtocolList", false), Sentence()),

        Production(Symbol("ProtocolDef", false), Sentence({Symbol("protocol", true), Symbol("id", true), Symbol("Extension", false),
                                                          Symbol("{", true), Symbol("ProtList", false), Symbol("}", true)})),

        Production(Symbol("Extension", false), Sentence({Symbol("extends", true), Symbol("id", true)})),
        Production(Symbol("Extension", false), Sentence()),

        Production(Symbol("ProtList", false), Sentence({Symbol("Prot", false), Symbol("ProtList", false)})),
        Production(Symbol("ProtList", false), Sentence()),

            //Tipado obligatorio
        Production(Symbol("Prot", false), Sentence({Symbol("id", true), Symbol("(", true), Symbol("TParams", false), Symbol(")", true), Symbol(":", true),
                                                   Symbol("TypeOrFunctor", false), Symbol(";", true)})),

        Production(Symbol("TParams", false), Sentence({Symbol("id", true), Symbol(":", true), Symbol("TypeOrFunctor", false), Symbol("TParamList", false)})),
        Production(Symbol("TParams", false), Sentence()),

        Production(Symbol("TParamList", false), Sentence({Symbol(",", true), Symbol("id", true), Symbol(":", true), Symbol("TypeOrFunctor", false), Symbol("TParamList", false)})),
        Production(Symbol("TParamList", false), Sentence()),



        //Gramática de definición de tipos
        Production(Symbol("TypeList", false), Sentence({Symbol("TypeDef", false), Symbol("TypeList", false)})),
        Production(Symbol("TypeList", false), Sentence()),

        Production(Symbol("TypeDef", false), Sentence({Symbol("type", true), Symbol("id", true), Symbol("TypeSuffix", false), Symbol("{", true),
                                                      Symbol("MemberList", false), Symbol("}", true)})),

        Production(Symbol("TypeSuffix", false), Sentence({Symbol("TypeParams", false), Symbol("Inheritance", false)})),


        Production(Symbol("TypeParams", false), Sentence({Symbol("(", true), Symbol("Params", false), Symbol(")", true)})),
        Production(Symbol("TypeParams", false), Sentence()),

        Production(Symbol("Inheritance", false), Sentence({Symbol("inherits", true), Symbol("id", true), Symbol("ParentParams", false)})),
        Production(Symbol("Inheritance", false), Sentence()),

        Production(Symbol("ParentParams", false), Sentence(Symbol("FuncCall", false))),
        Production(Symbol("ParentParams", false), Sentence()),

        Production(Symbol("MemberList", false), Sentence({Symbol("id", true), Symbol("Next", false), Symbol("MemberList", false)})),
        Production(Symbol("MemberList", false), Sentence()),

        Production(Symbol("Next", false), Sentence({Symbol("=", true), Symbol("Value", false), Symbol(";", true)})),
        Production(Symbol("Next", false), Sentence({Symbol("(", true), Symbol("Params", false), Symbol(")", true), Symbol("Typing", false), Symbol("FuncBody", false)})),



        //Gramatica de condicional
        Production(Symbol("Condition", false), Sentence({Symbol("(", true), Symbol("Expr", false), Symbol(")", true), Symbol("Body", false)})),

        Production(Symbol("CondExpr", false), Sentence({Symbol("if", true), Symbol("Condition", false), Symbol("CondList", false),
                                                       Symbol("else", true), Symbol("Body", false)})),

        Production(Symbol("CondList", false), Sentence({Symbol("elif", true), Symbol("Condition", false), Symbol("CondList", false)})),
        Production(Symbol("CondList", false), Sentence()),



        //Gramatica de ciclos
        Production(Symbol("LoopExpr", false), Sentence(Symbol("WhileExpr", false))),
        Production(Symbol("LoopExpr", false), Sentence(Symbol("ForExpr", false))),

        Production(Symbol("WhileExpr", false), Sentence({Symbol("while", true), Symbol("Condition", false)})),


        Production(Symbol("ForExpr", false), Sentence({Symbol("for", true), Symbol("(", true), Symbol("id", true), Symbol("in", true), Symbol("Collection", false),
                                                      Symbol(")", true), Symbol("Body", false)})),


        Production(Symbol("Collection", false), Sentence({Symbol("range", true), Symbol("(", true), Symbol("number", true), Symbol(",", true), Symbol("number", true),
                                                         Symbol(")", true)})),
        Production(Symbol("Collection", false), Sentence(Symbol("id", true))),


        //Gramatica de asignación
        Production(Symbol("AssignExpr", false), Sentence({Symbol("let", true), Symbol("AssignList", false), Symbol("in", true), Symbol("Body", false)})),

        Production(Symbol("AssignList", false), Sentence({Symbol("Assignation", false), Symbol("AssignList_", false)})),
        Production(Symbol("AssignList_", false), Sentence({Symbol(",", true), Symbol("Assignation", false), Symbol("AssignList_", false)})),
        Production(Symbol("AssignList_", false), Sentence()),


        Production(Symbol("Assignation", false), Sentence({Symbol("id", true), Symbol("Typing", false), Symbol("=", true), Symbol("Value", false)})),

        Production(Symbol("Value", false), Sentence(Symbol("Expression", false))),
        Production(Symbol("Value", false), Sentence({Symbol("new", true), Symbol("id", true), Symbol("FuncCall", false)})),
        Production(Symbol("Value", false), Sentence({Symbol("[", true), Symbol("Vector", false), Symbol("]", true)})),
        Production(Symbol("Value", false), Sentence({Symbol("range", true), Symbol("(", true), Symbol("number", true), Symbol(",", true), Symbol("number", true),
                                                         Symbol(")", true)})),

        Production(Symbol("Vector", false), Sentence({Symbol("AltCheckExpr", false), Symbol("VecInit", false)})), //Un vector no puede empezar con una operación |
        Production(Symbol("Vector", false), Sentence()),

        Production(Symbol("VecInit", false), Sentence({Symbol("|", true), Symbol("id", true), Symbol("in", true), Symbol("Collection", false)})),
        Production(Symbol("VecInit", false), Sentence(Symbol("ElemList", false))),

        Production(Symbol("ElemList", false), Sentence({Symbol(",", true), Symbol("Expr", false), Symbol("ElemList", false)})),
        Production(Symbol("ElemList", false), Sentence()),

        Production(Symbol("AltCheckExpr", false), Sentence({Symbol("AndExpr", false), Symbol("AltCheckExpr_", false)})),
        Production(Symbol("AltCheckExpr_", false), Sentence({Symbol("is", true), Symbol("Type", false)})),
        Production(Symbol("AltCheckExpr_", false), Sentence()),



        //Gramática de tipado
        Production(Symbol("Typing", false), Sentence({Symbol(":", true), Symbol("TypeOrFunctor", false)})),
        Production(Symbol("Typing", false), Sentence()),

        Production(Symbol("TypeOrFunctor", false), Sentence(Symbol("Type", false))),
        Production(Symbol("TypeOrFunctor", false), Sentence(Symbol("Functor", false))),

        Production(Symbol("Type", false), Sentence({Symbol("Number", true), Symbol("SubType", false)})),
        Production(Symbol("Type", false), Sentence({Symbol("String", true), Symbol("SubType", false)})),
        Production(Symbol("Type", false), Sentence({Symbol("Boolean", true), Symbol("SubType", false)})),
        Production(Symbol("Type", false), Sentence({Symbol("id", true), Symbol("SubType", false)})),

        Production(Symbol("SubType", false), Sentence(Symbol("*", true))),
        Production(Symbol("SubType", false), Sentence({Symbol("[", true), Symbol("]", true)})),
        Production(Symbol("SubType", false), Sentence()),

        Production(Symbol("Functor", false), Sentence({Symbol("(", true), Symbol("FParams", false), Symbol(")", true), Symbol("->", true), Symbol("Type", false)})),

        Production(Symbol("FParams", false), Sentence({Symbol("Type", false), Symbol("FParamList", false)})),
        Production(Symbol("FParams", false), Sentence()),

        Production(Symbol("FParamList", false), Sentence({Symbol(",", true), Symbol("Type", false), Symbol("ParamList", false)})),
        Production(Symbol("FParamList", false), Sentence()),


        //Gramática de Funciones
        Production(Symbol("Params", false), Sentence({Symbol("id", true), Symbol("Typing", false), Symbol("ParamList", false)})),
        Production(Symbol("Params", false), Sentence()),

        Production(Symbol("ParamList", false), Sentence({Symbol(",", true),Symbol("id", true), Symbol("Typing", false), Symbol("ParamList", false)})),
        Production(Symbol("ParamList", false), Sentence()),

        Production(Symbol("Function", false), Sentence({Symbol("function", true), Symbol("id", true), Symbol("(", true), Symbol("Params", false), Symbol(")", true),
                                                       Symbol("Typing", false), Symbol("FuncBody", false)})),

        Production(Symbol("FuncBody", false), Sentence({Symbol("=>", true), Symbol("Expression", false), Symbol(";", true)})),
        Production(Symbol("FuncBody", false), Sentence({Symbol("ExprBlock", false)})),

        Production(Symbol("FuncList", false), Sentence({Symbol("FuncOrMacro", false), Symbol("FuncList", false)})),
        Production(Symbol("FuncList", false), Sentence()),

        Production(Symbol("FuncOrMacro", false), Sentence(Symbol("Function", false))),
        Production(Symbol("FuncOrMacro", false), Sentence(Symbol("Macro", false))),


         //Gramática de Macros
        Production(Symbol("MParams", false), Sentence({Symbol("ArgModif", false), Symbol("Typing", false), Symbol("MParamList", false)})),
        Production(Symbol("MParams", false), Sentence()),

        Production(Symbol("MParamList", false), Sentence({Symbol(",", true),Symbol("ArgModif", false), Symbol("Typing", false), Symbol("MParamList", false)})),
        Production(Symbol("MParamList", false), Sentence()),

        Production(Symbol("Macro", false), Sentence({Symbol("def", true), Symbol("id", true), Symbol("(", true), Symbol("MParams", false), Symbol(")", true),
                                                    Symbol("Typing", false), Symbol("FuncBody", false)})),

        Production(Symbol("ArgModif", false), Sentence({Symbol("*", true), Symbol("id", true)})),
        Production(Symbol("ArgModif", false), Sentence({Symbol("@", true), Symbol("id", true)})),
        Production(Symbol("ArgModif", false), Sentence({Symbol("$", true), Symbol("id", true)})),
        Production(Symbol("ArgModif", false), Sentence(Symbol("id", true))),


        //Gramática de operadores
        Production(Symbol("Reassignment", false), Sentence({Symbol("id", false), Symbol(":=", true), Symbol("Expr", false)})),

        Production(Symbol("Expr", false), Sentence(Symbol("CheckExpr", false))),
        Production(Symbol("CheckExpr", false), Sentence({Symbol("OrExpr", false), Symbol("CheckExpr_", false)})),
        Production(Symbol("CheckExpr_", false), Sentence({Symbol("is", true), Symbol("Type", false)})),
        Production(Symbol("CheckExpr_", false), Sentence()),
        Production(Symbol("OrExpr", false), Sentence({Symbol("AndExpr", false), Symbol("OrExpr_", false)})),
        Production(Symbol("OrExpr_", false), Sentence({Symbol("|", true), Symbol("AndExpr", false), Symbol("OrExpr_", false)})),
        Production(Symbol("OrExpr_", false), Sentence()),
        Production(Symbol("AndExpr", false), Sentence({Symbol("EqualityExpr", false), Symbol("AndExpr_", false)})),
        Production(Symbol("AndExpr_", false), Sentence({Symbol("&", true), Symbol("EqualityExpr", false), Symbol("AndExpr_", false)})),
        Production(Symbol("AndExpr_", false), Sentence()),
        Production(Symbol("EqualityExpr", false), Sentence({Symbol("CompareExpr", false), Symbol("EqualityExpr_", false)})),
        Production(Symbol("EqualityExpr_", false), Sentence({Symbol("==", true), Symbol("CompareExpr", false), Symbol("EqualityExpr_", false)})),
        Production(Symbol("EqualityExpr_", false), Sentence({Symbol("!=", true), Symbol("CompareExpr", false), Symbol("EqualityExpr_", false)})),
        Production(Symbol("EqualityExpr_", false), Sentence()),
//        Production(Symbol("ConcatExpr", false), Sentence({Symbol("ArithExpr", false), Symbol("ConcatExpr_", false)})),
//        Production(Symbol("ConcatExpr_", false), Sentence({Symbol("@", true), Symbol("ArithExpr", false), Symbol("ConcatExpr_", false)})),
//        Production(Symbol("ConcatExpr_", false), Sentence()),
        Production(Symbol("CompareExpr", false), Sentence({Symbol("ArithExpr", false), Symbol("CompareExpr_", false)})),
        Production(Symbol("CompareExpr_", false), Sentence({Symbol(">", true), Symbol("ArithExpr", false), Symbol("CompareExpr_", false)})),
        Production(Symbol("CompareExpr_", false), Sentence({Symbol("<", true), Symbol("ArithExpr", false), Symbol("CompareExpr_", false)})),
        Production(Symbol("CompareExpr_", false), Sentence({Symbol(">=", true), Symbol("ArithExpr", false), Symbol("CompareExpr_", false)})),
        Production(Symbol("CompareExpr_", false), Sentence({Symbol("<=", true), Symbol("ArithExpr", false), Symbol("CompareExpr_", false)})),
        Production(Symbol("CompareExpr_", false), Sentence()),
        Production(Symbol("ArithExpr", false), Sentence({Symbol("Term", false), Symbol("ArithExpr_", false)})),
        Production(Symbol("ArithExpr_", false), Sentence({Symbol("+", true), Symbol("Term", false), Symbol("ArithExpr_", false)})),
        Production(Symbol("ArithExpr_", false), Sentence({Symbol("-", true), Symbol("Term", false), Symbol("ArithExpr_", false)})),
        Production(Symbol("ArithExpr_", false), Sentence()),
        Production(Symbol("Term", false), Sentence({Symbol("Pot", false), Symbol("Term_", false)})),
        Production(Symbol("Term_", false), Sentence({Symbol("*", true), Symbol("Pot", false), Symbol("Term_", false)})),
        Production(Symbol("Term_", false), Sentence({Symbol("/", true), Symbol("Pot", false), Symbol("Term_", false)})),
        Production(Symbol("Term_", false), Sentence()),
        Production(Symbol("Pot", false), Sentence({Symbol("UnaryExpr", false), Symbol("Pot_", false)})),
        Production(Symbol("Pot_", false), Sentence({Symbol("^", true), Symbol("UnaryExpr", false), Symbol("Pot_", false)})),
        Production(Symbol("Pot_", false), Sentence({Symbol("%", true), Symbol("UnaryExpr", false), Symbol("Pot_", false)})),
        Production(Symbol("Pot_", false), Sentence()),
        Production(Symbol("UnaryExpr", false), Sentence({Symbol("!", true), Symbol("Factor", false)})),
        Production(Symbol("UnaryExpr", false), Sentence({Symbol("-", true), Symbol("Factor", false)})), //Operador negativo
        Production(Symbol("UnaryExpr", false), Sentence(Symbol("Factor", false))),
        Production(Symbol("Factor", false), Sentence({Symbol("(", true), Symbol("Expr", false), Symbol(")", true)})),
        Production(Symbol("Factor", false), Sentence({Symbol("number", true)})),
        Production(Symbol("Factor", false), Sentence({Symbol("true", true)})),
        Production(Symbol("Factor", false), Sentence({Symbol("false", true)})),
        Production(Symbol("Factor", false), Sentence({Symbol("string", true)})),
        Production(Symbol("Factor", false), Sentence({Symbol("id", true), Symbol("Follow", false)})),
        Production(Symbol("Follow", false), Sentence(Symbol("FuncCall", false))),
        Production(Symbol("Follow", false), Sentence({Symbol(".", true), Symbol("id", true), Symbol("Follow", false)})),
        Production(Symbol("Follow", false), Sentence({Symbol("[", true), Symbol("ArithExpr", false), Symbol("]", true)})),
        Production(Symbol("Follow", false), Sentence()),
        Production(Symbol("FuncCall", false), Sentence({Symbol("(", true), Symbol("CallParams", false), Symbol(")", true)})),
        Production(Symbol("CallParams", false), Sentence({Symbol("SymbolicParams", false), Symbol("CallList", false)})),
        Production(Symbol("CallParams", false), Sentence()),
        Production(Symbol("CallList", false), Sentence({Symbol(",", true), Symbol("SymbolicParams", false), Symbol("CallList", false)})),
        Production(Symbol("CallList", false), Sentence()),
        Production(Symbol("SymbolicParams", false), Sentence({Symbol("@", true), Symbol("id", true), Symbol("SymbolicFollow", false)})),
        Production(Symbol("SymbolicParams", false), Sentence(Symbol("Expr", false))),
        Production(Symbol("SymbolicFollow", false), Sentence({Symbol(".", true), Symbol("id", true), Symbol("Follow", false)})),
        Production(Symbol("SymbolicFollow", false), Sentence())
    };

    //Inserciones
{
    //Macro-relacionado
    nonTerminals.insert(Symbol("Macro", false));
    nonTerminals.insert(Symbol("MParams", false));
    nonTerminals.insert(Symbol("MParamList", false));
    nonTerminals.insert(Symbol("ArgModif", false));


    //Tipo-relacionado
    nonTerminals.insert(Symbol("Value", false));
    nonTerminals.insert(Symbol("TypeSuffix", false));
    nonTerminals.insert(Symbol("ParentParams", false));
    nonTerminals.insert(Symbol("Next", false));
    nonTerminals.insert(Symbol("TParams", false));
    nonTerminals.insert(Symbol("TParamList", false));
    nonTerminals.insert(Symbol("FuncBody", false));
    nonTerminals.insert(Symbol("FuncOrMacro", false));

    //Tipado-relacionado
    nonTerminals.insert(Symbol("Typing", false));
    nonTerminals.insert(Symbol("Type", false));
    nonTerminals.insert(Symbol("SubType", false));
    nonTerminals.insert(Symbol("TypeOrFunctor", false));
    nonTerminals.insert(Symbol("Functor", false));
    nonTerminals.insert(Symbol("FParams", false));
    nonTerminals.insert(Symbol("FParamList", false));


    //Expresiones
    nonTerminals.insert(Symbol("CheckExpr", false));
    nonTerminals.insert(Symbol("CheckExpr_", false));
    nonTerminals.insert(Symbol("AltCheckExpr", false));
    nonTerminals.insert(Symbol("AltCheckExpr_", false));
    nonTerminals.insert(Symbol("OrExpr", false));
    nonTerminals.insert(Symbol("OrExpr_", false));
    nonTerminals.insert(Symbol("AndExpr", false));
    nonTerminals.insert(Symbol("AndExpr_", false));
    nonTerminals.insert(Symbol("EqualityExpr", false));
    nonTerminals.insert(Symbol("EqualityExpr_", false));
    nonTerminals.insert(Symbol("CompareExpr", false));
    nonTerminals.insert(Symbol("CompareExpr_", false));
    nonTerminals.insert(Symbol("ArithExpr", false));
    nonTerminals.insert(Symbol("ArithExpr_", false));
    nonTerminals.insert(Symbol("Term", false));
    nonTerminals.insert(Symbol("Term_", false));
    nonTerminals.insert(Symbol("Pot", false));
    nonTerminals.insert(Symbol("Pot_", false));
    nonTerminals.insert(Symbol("UnaryExpr", false));
    nonTerminals.insert(Symbol("Factor", false));
    nonTerminals.insert(Symbol("Follow", false));
    nonTerminals.insert(Symbol("FuncCall", false));
    nonTerminals.insert(Symbol("CallParams", false));
    nonTerminals.insert(Symbol("CallList", false));
    nonTerminals.insert(Symbol("SymbolicParams", false));
    nonTerminals.insert(Symbol("SymbolicFollow", false));

    //Listas
    nonTerminals.insert(Symbol("TypeList", false));
    nonTerminals.insert(Symbol("MemberList", false));
    nonTerminals.insert(Symbol("ExprList", false));
    nonTerminals.insert(Symbol("FuncList", false));
    nonTerminals.insert(Symbol("ProtocolList", false));
    nonTerminals.insert(Symbol("ProtList", false));
    nonTerminals.insert(Symbol("ParamList", false));
    nonTerminals.insert(Symbol("ElemList", false));
    nonTerminals.insert(Symbol("ExprList", false));
    nonTerminals.insert(Symbol("AssignList", false));
    nonTerminals.insert(Symbol("AssignList_", false));
    nonTerminals.insert(Symbol("CondList", false));
}


    return Grammar(terminals, nonTerminals, Symbol("Program", false), productions);
}








/*
TODO ShiftReduce-Parser
*/

/*
TODO SLR1-Parser(ShiftReduce-Parser)
*/









//======================================== TEST ========================================

void test_LL1Parser(vector<Symbol> input, Grammar grammar) {

    try {
        LL1Parser parser(grammar);


        vector<Production> result = parser.parse(input);
        parser.printParseResult(result);

    } catch (const exception& e) {
        cout << "Error: " << e.what() << endl;
    }
}
