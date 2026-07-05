#pragma once
#include <iostream>
#include <tuple>
#include <memory>
#include <string>

using namespace std;

// ── Nodos ────────────────────────────────────────────────────────────────────

//Clase base abstracta
class Node {
public:
    char value;
    Node() : value('\0') {}
    virtual ~Node() = default;
    virtual string getType() const { return "Node"; }
    virtual void print() { cout << value; }
    virtual void printAST(int indent = 0) const = 0;
    virtual string getValue() { return string(1, value); }
};

class SymbolNode : public Node {
public:
    SymbolNode(char c) { value = c; }
    void printAST(int indent = 0) const override {
        cout << string(indent, ' ') << "Symbol('" << value << "')" << endl;
    }
};

class ClosureNode : public Node {
public:
    unique_ptr<Node> child;
    ClosureNode(unique_ptr<Node> node) : child(move(node)) { value = '*'; }
    string getType() const override { return "ClosureNode"; }
    void print() override { child->Node::print(); cout << " * "; }
    void printAST(int indent = 0) const override {
        cout << string(indent, ' ') << "Closure(*):" << endl;
        child->printAST(indent + 4);
    }
};

class ConcatNode : public Node {
public:
    string value;
    unique_ptr<Node> left, right;
    ConcatNode(unique_ptr<Node> l, unique_ptr<Node> r) : left(move(l)), right(move(r)) {
        value = left->getValue() + right->getValue();
    }
    string getType() const override { return "ConcatNode"; }
    string getValue() override { return value; }
    void printAST(int indent = 0) const override {
        cout << string(indent, ' ') << "Concat(.):" << endl;
        left->printAST(indent + 4);
        right->printAST(indent + 4);
    }
};

class UnionNode : public Node {
public:
    unique_ptr<Node> left, right;
    UnionNode(unique_ptr<Node> l, unique_ptr<Node> r) : left(move(l)), right(move(r)) { value = '|'; }
    string getType() const override { return "UnionNode"; }
    void print() override { left->Node::print(); cout << " | "; right->Node::print(); }
    void printAST(int indent = 0) const override {
        cout << string(indent, ' ') << "Union(|):" << endl;
        left->printAST(indent + 4);
        right->printAST(indent + 4);
    }
};

class GroupNode : public Node {
public:
    string value;
    unique_ptr<Node> expr;
    GroupNode(unique_ptr<Node> e) : expr(move(e)) { value = expr->getValue(); }
    string getType() const override { return "GroupNode"; }
    void print() override { expr->Node::print(); }
    void printAST(int indent = 0) const override {
        cout << string(indent, ' ') << "Group(()):" << endl;
        expr->printAST(indent + 4);
    }
};

// ── RegEx ─────────────────────────────────────────────────────────────────────

class RegEx {
private:
    string input;
    size_t pos = 0;

    char peek() { return (pos < input.size()) ? input[pos] : '\0'; }
    void consume() { pos++; }

    unique_ptr<Node> parseTerm() {
        auto left = parseFactor();
        while (peek() != '\0' && peek() != '|' && peek() != ')')
            left = make_unique<ConcatNode>(move(left), parseFactor());
        return left;
    }

    unique_ptr<Node> parseFactor() {
        auto node = parseBase();
        if (peek() == '*') { consume(); node = make_unique<ClosureNode>(move(node)); }
        return node;
    }

    unique_ptr<Node> parseBase() {
        if (peek() == '\\') {
            consume();
            if (peek() == '\0') throw runtime_error("Escape incompleto al final de la regex");
            char c = peek(); consume();
            switch (c) {
                case 'n': return make_unique<SymbolNode>('\n');
                case 't': return make_unique<SymbolNode>('\t');
                case 'v': return make_unique<SymbolNode>('\v');
                case 'r': return make_unique<SymbolNode>('\r');
                case 'f': return make_unique<SymbolNode>('\f');
                default:  return make_unique<SymbolNode>(c);
            }
        }
        if (peek() == '(') {
            consume();
            auto node = parseRegex();
            if (peek() != ')') throw runtime_error("Missing ')'");
            consume();
            return make_unique<GroupNode>(move(node));
        }
        char c = peek(); consume();
        return make_unique<SymbolNode>(c);
    }

public:
    RegEx(string input_) : input(input_) {}
    unique_ptr<Node> parseRegex() {
        auto left = parseTerm();
        while (peek() == '|') {
            consume();
            left = make_unique<UnionNode>(move(left), parseTerm());
        }
        return left;
    }
};
