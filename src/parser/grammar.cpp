#include <iostream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>

using namespace std;


const string EPSILON = "ε"; 


/**
 * Representa un símbolo de la gramática (terminal o no terminal)
 */
class Symbol {
private:
    string name;
    bool is_terminal;

public:
    Symbol() = default;
    Symbol(const string& name, bool is_terminal = false) 
        : name(name), is_terminal(is_terminal) {}
    
    const string& getName() const { return name; }
    bool isTerminal() const { return is_terminal; }
    bool isNonTerminal() const { return !is_terminal; }
    
    bool operator==(const Symbol& other) const {
        return name == other.name && is_terminal == other.is_terminal;
    }
    bool operator<(const Symbol& other) const {
        return name < other.name;
    }
};

// Hash para Symbol
namespace std {
    template<>
    struct hash<Symbol> {
        size_t operator()(const Symbol& s) const {
            return hash<string>()(s.getName()) ^ hash<bool>()(s.isTerminal());
        }
    };
}

/**
 * Sentence
 *      Representa una sentencia (secuencia de símbolos)
 */
class Sentence {
private:
    vector<Symbol> symbols;
    
public:
    Sentence() = default;
    Sentence(const vector<Symbol>& syms) : symbols(syms) {}
    Sentence(const Symbol& single_symbol) : symbols({single_symbol}) {}
    
    bool isEpsilon() const {
        return symbols.empty();
    }
    
    size_t size() const { return symbols.size(); }
    
    const Symbol& operator[](size_t index) const { return symbols[index]; }
    
    auto begin() const { return symbols.begin(); }
    auto end() const { return symbols.end(); }
    
    bool operator==(const Sentence& other) const {
        return symbols == other.symbols;
    }
    
    string toString() const {
        if (isEpsilon()) return EPSILON;
        
        string result;
        for (size_t i = 0; i < symbols.size(); ++i) {
            if (i > 0) result += " ";
            result += symbols[i].getName();
        }
        return result;
    }
};

// Hash para Sentence
namespace std {
    template<>
    struct hash<Sentence> {
        size_t operator()(const Sentence& s) const {
            size_t result = 0;
            for (const auto& sym : s) {
                result ^= hash<string>()(sym.getName()) ^ (hash<bool>()(sym.isTerminal()) << 1);
            }
            return result;
        }
    };
}

/**
 * ContainerSet
 *      Representa el conjunto que puede contener símbolos terminales y epsilon
 */
class ContainerSet {
private:
    unordered_set<Symbol> symbols;
    bool has_epsilon;
    
public:
    ContainerSet() : has_epsilon(false) {}
    
    ContainerSet(const Symbol& symbol) : has_epsilon(false) {
        if (symbol.getName() == EPSILON || symbol.getName() == "epsilon") {
            has_epsilon = true;
        } else {
            symbols.insert(symbol);
        }
    }
    
    void setEpsilon() {
        has_epsilon = true;
    }
    
    bool containsEpsilon() const {
        return has_epsilon;
    }
    
    void insert(const Symbol& symbol) {
        if (symbol.getName() == EPSILON || symbol.getName() == "epsilon") {
            has_epsilon = true;
        } else {
            symbols.insert(symbol);
        }
    }
    
    void update(const ContainerSet& other) {
        for (const auto& symbol : other.symbols) {
            symbols.insert(symbol);
        }
        if (other.has_epsilon) {
            has_epsilon = true;
        }
    }
    
    bool hardUpdate(const ContainerSet& other) {
        size_t old_size = symbols.size();
        bool old_epsilon = has_epsilon;
        
        update(other);
        
        return symbols.size() != old_size || has_epsilon != old_epsilon;
    }
    
    ContainerSet operator-(const unordered_set<string>& to_remove) const {
        ContainerSet result;
        
        for (const auto& symbol : symbols) {
            if (to_remove.find(symbol.getName()) == to_remove.end()) {
                result.insert(symbol);
            }
        }
        
        if (has_epsilon && to_remove.find(EPSILON) == to_remove.end() && 
            to_remove.find("epsilon") == to_remove.end()) {
            result.setEpsilon();
        }
        
        return result;
    }
    
    void print() const {
        cout << "{ ";
        for (const auto& symbol : symbols) {
            cout << symbol.getName() << " ";
        }
        if (has_epsilon) {
            cout << EPSILON << " ";
        }
        cout << "}";
    }
    
    bool empty() const {
        return symbols.empty() && !has_epsilon;
    }
    
    const unordered_set<Symbol>& getSymbols() const { return symbols; }
};



/**
 * Production
 *      Representa una producción de la gramática
 */
class Production {
private:
    Symbol left;
    Sentence right;
    
public:
    Production() = default;
    Production(const Production& other) 
        : left(other.left), right(other.right) {}
    Production(const Symbol& left, const Sentence& right) 
        : left(left), right(right) {}
    
    const Symbol& getLeft() const { return left; }
    const Sentence& getRight() const { return right; }
    
    string toString() const {
        return left.getName() + " -> " + right.toString();
    }
    
    bool operator==(const Production& other) const {
        return left == other.left && right == other.right;
    }
};

/**
 * Grammar
 *      Representa una gramatica libre de contexto
 */
class Grammar {
private:
    unordered_set<Symbol> terminals;
    unordered_set<Symbol> nonTerminals;
    Symbol startSymbol;
    vector<Production> productions;
    
public:
    Grammar(const unordered_set<Symbol>& terminals,
            const unordered_set<Symbol>& nonTerminals,
            const Symbol& startSymbol,
            const vector<Production>& productions)
        : terminals(terminals), nonTerminals(nonTerminals), 
          startSymbol(startSymbol), productions(productions) {}
    
    const unordered_set<Symbol>& getTerminals() const { return terminals; }
    const unordered_set<Symbol>& getNonTerminals() const { return nonTerminals; }
    const Symbol& getStartSymbol() const { return startSymbol; }
    const vector<Production>& getProductions() const { return productions; }
    
    void print() const {
        cout << "Gramatica:\n";
        cout << "Terminales: ";
        for (const auto& t : terminals) {
            cout << t.getName() << " ";
        }
        cout << "\nNo terminales: ";
        for (const auto& nt : nonTerminals) {
            cout << nt.getName() << " ";
        }
        cout << "\nSimbolo inicial: " << startSymbol.getName();
        cout << "\nProducciones:\n";
        for (const auto& prod : productions) {
            cout << "  " << prod.toString() << "\n";
        }
    }
};

