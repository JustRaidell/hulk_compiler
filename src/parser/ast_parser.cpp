using namespace std;
#include "../semantics/visitor.h"

// ─── Nodos AST ────────────────────────────────────────────────────────────────

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(int indent = 0) const = 0;
    virtual void accept (Visitor& v) = 0;
    string inferred_type;
};

struct ProgramNode : ASTNode {
    vector<unique_ptr<ASTNode>> declarations;  // functions, types, protocols
    unique_ptr<ASTNode> body;                  // la expresión principal, obligatoria
    ProgramNode(vector<unique_ptr<ASTNode>> declarations, unique_ptr<ASTNode> body)
        : declarations(move(declarations)), body(move(body)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Program\n";
        for (const auto& decl : declarations)
            decl->print(indent + 2);
        cout << string(indent + 2, ' ') << "Body\n";
        body->print(indent + 4);
    }
};

struct NumberNode : ASTNode {
    double value;
    NumberNode(double v) : value(v) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Number(" << value << ")\n";
    }
};

struct BoolNode : ASTNode {
    bool value;
    BoolNode(bool v) : value(v) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Bool(" << (value ? "true" : "false") << ")\n";
    }
};

struct StringNode : ASTNode {
    string value;
    StringNode(const string& s) : value(s) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "String(" << value << ")\n";
    }
};

struct IdentifierNode : ASTNode {
    string name;
    IdentifierNode(const string& n) : name(n) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Id(" << name << ")\n";
    }
};

struct IsNode : ASTNode {
    unique_ptr<ASTNode> expr;
    string type_name;
    IsNode(unique_ptr<ASTNode> expr, const string& type_name)
        : expr(move(expr)), type_name(type_name) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Is(" << type_name << ")\n";
        expr->print(indent + 2);
    }
};

struct BinaryOpNode : ASTNode {
    string op;
    unique_ptr<ASTNode> left;
    unique_ptr<ASTNode> right;
    BinaryOpNode(const string& op, unique_ptr<ASTNode> l, unique_ptr<ASTNode> r)
        : op(op), left(move(l)), right(move(r)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "BinaryOp(" << op << ")\n";
        left->print(indent + 2);
        right->print(indent + 2);
    }
};

struct UnaryOpNode : ASTNode {
    string op;
    unique_ptr<ASTNode> operand;
    UnaryOpNode(const string& op, unique_ptr<ASTNode> operand)
        : op(op), operand(move(operand)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "UnaryOp(" << op << ")\n";
        operand->print(indent + 2);
    }
};

struct ReassignNode : ASTNode {
    unique_ptr<ASTNode> target;
    unique_ptr<ASTNode> value;
    ReassignNode(unique_ptr<ASTNode> target, unique_ptr<ASTNode> value)
        : target(move(target)), value(move(value)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Reassign\n";
        target->print(indent + 2);
        value->print(indent + 2);
    }
};

struct CallNode : ASTNode {
    unique_ptr<ASTNode> callee;
    vector<unique_ptr<ASTNode>> args;

    CallNode(unique_ptr<ASTNode> callee, vector<unique_ptr<ASTNode>> args)
        : callee(move(callee)), args(move(args)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Call\n";
        callee->print(indent + 2);
        for (const auto& arg : args)
            arg->print(indent + 2);
    }
};

struct MemberAccessNode : ASTNode {
    unique_ptr<ASTNode> object;
    string member;

    MemberAccessNode(unique_ptr<ASTNode> object, const string& member)
        : object(move(object)), member(member) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "MemberAccess(." << member << ")\n";
        object->print(indent + 2);
    }
};

struct IndexNode : ASTNode {
    unique_ptr<ASTNode> object;
    unique_ptr<ASTNode> index;

    IndexNode(unique_ptr<ASTNode> object, unique_ptr<ASTNode> index)
        : object(move(object)), index(move(index)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Index\n";
        object->print(indent + 2);
        index->print(indent + 2);
    }
};

struct VectorNode : ASTNode {
    vector<unique_ptr<ASTNode>> elements;
    VectorNode(vector<unique_ptr<ASTNode>> elements)
        : elements(move(elements)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Vector\n";
        for (const auto& el : elements)
            el->print(indent + 2);
    }
};

struct VectorGeneratorNode : ASTNode {
    unique_ptr<ASTNode> expr;
    string variable;
    unique_ptr<ASTNode> iterable;
    VectorGeneratorNode(unique_ptr<ASTNode> expr, const string& variable,
                        unique_ptr<ASTNode> iterable)
        : expr(move(expr)), variable(variable), iterable(move(iterable)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "VectorGenerator(" << variable << ")\n";
        expr->print(indent + 2);
        iterable->print(indent + 2);
    }
};

struct LetInNode : ASTNode {
    vector<tuple<string, string, unique_ptr<ASTNode>>> bindings;  // [(name, type, value), ...]
    unique_ptr<ASTNode> body;
    LetInNode(vector<tuple<string, string, unique_ptr<ASTNode>>> bindings, unique_ptr<ASTNode> body)
        : bindings(move(bindings)), body(move(body)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "LetIn\n";
        for (const auto& [name, type, val] : bindings) {
            cout << string(indent + 2, ' ') << "Bind(" << name << (type == "" ? "" : ": " ) << type << ")\n";
            val->print(indent + 4);
        }
        cout << string(indent + 2, ' ') << "Body\n";
        body->print(indent + 4);
    }
};

struct IfNode : ASTNode {
    vector<pair<unique_ptr<ASTNode>, unique_ptr<ASTNode>>> branches;  // [(cond, body), ...]
    unique_ptr<ASTNode> else_body;
    IfNode(vector<pair<unique_ptr<ASTNode>, unique_ptr<ASTNode>>> branches,
           unique_ptr<ASTNode> else_body)
        : branches(move(branches)), else_body(move(else_body)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "If\n";
        for (const auto& [cond, body] : branches) {
            cout << string(indent + 2, ' ') << "Branch\n";
            cond->print(indent + 4);
            body->print(indent + 4);
        }
        if (else_body) {
            cout << string(indent + 2, ' ') << "Else\n";
            else_body->print(indent + 4);
        }
    }
};

struct WhileNode : ASTNode {
    unique_ptr<ASTNode> condition;
    unique_ptr<ASTNode> body;
    WhileNode(unique_ptr<ASTNode> condition, unique_ptr<ASTNode> body)
        : condition(move(condition)), body(move(body)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "While\n";
        condition->print(indent + 2);
        body->print(indent + 2);
    }
};

struct ForNode : ASTNode {
    string variable;
    unique_ptr<ASTNode> iterable;
    unique_ptr<ASTNode> body;
    ForNode(const string& variable, unique_ptr<ASTNode> iterable, unique_ptr<ASTNode> body)
        : variable(variable), iterable(move(iterable)), body(move(body)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "For(" << variable << ")\n";
        iterable->print(indent + 2);
        body->print(indent + 2);
    }
};

struct BlockNode : ASTNode {
    vector<unique_ptr<ASTNode>> expressions;
    BlockNode(vector<unique_ptr<ASTNode>> expressions)
        : expressions(move(expressions)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Block\n";
        for (const auto& expr : expressions)
            expr->print(indent + 2);
    }
};

struct FunctionNode : ASTNode {
    string name;
    vector<pair<string, string>> params;  // [(name, type)] — type "" si no anotado
    string return_type;                   // "" si no anotado
    unique_ptr<ASTNode> body;             // nullptr si es declaración de protocolo
    bool is_macro;


    FunctionNode(const string& name,
                 vector<pair<string, string>> params,
                 const string& return_type,
                 unique_ptr<ASTNode> body,
                 bool macro = false)
        : name(name), params(move(params)), return_type(return_type), body(move(body)) {this->is_macro = macro;}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << (is_macro ? "Macro(" : "Function(") << name << ")\n";
        cout << string(indent + 2, ' ') << "Params(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) cout << ", ";
            cout << params[i].first;
            if (!params[i].second.empty())
                cout << ": " << params[i].second;
        }
        cout << ")\n";
        if (!return_type.empty())
            cout << string(indent + 2, ' ') << "Returns(" << return_type << ")\n";
        if (body)
            body->print(indent + 2);
    }
};

struct TypeNode : ASTNode {
    string name;
    vector<pair<string, string>> params;
    string parent;
    vector<unique_ptr<ASTNode>> parent_args;
    vector<tuple<string, string, unique_ptr<ASTNode>>> attributes;
    vector<unique_ptr<ASTNode>> methods;
    vector<string> satisfied_protocols;

    TypeNode(const string& name,
             vector<pair<string, string>> params,
             const string& parent,
             vector<unique_ptr<ASTNode>> parent_args,
             vector<tuple<string, string, unique_ptr<ASTNode>>> attributes,
             vector<unique_ptr<ASTNode>> methods)
        : name(name), params(move(params)), parent(parent),
          parent_args(move(parent_args)), attributes(move(attributes)),
          methods(move(methods)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Type(" << name << ")\n";
        if (!params.empty()) {
            cout << string(indent + 2, ' ') << "Params(";
            for (size_t i = 0; i < params.size(); ++i) {
                if (i > 0) cout << ", ";
                cout << params[i].first << (params[i].second == "" ? "" : ": " ) << params[i].second;
            }
            cout << ")\n";
        }
        if (!parent.empty()) {
            cout << string(indent + 2, ' ') << "Inherits(" << parent << ")\n";
            for (const auto& arg : parent_args)
                arg->print(indent + 4);
        }
        for (const auto& [name, type, val] : attributes) {
            cout << string(indent + 2, ' ') << "Attribute(" << name << (type == "" ? "" : ": " ) << type << ")\n";
            val->print(indent + 4);
        }
        for (const auto& method : methods)
            method->print(indent + 2);
    }
};

struct ProtocolNode : ASTNode {
    string name;
    string parent;                           // "" si no extiende ninguno
    vector<unique_ptr<ASTNode>> methods;     // FunctionNode con body == nullptr

    ProtocolNode(const string& name, const string& parent,
                 vector<unique_ptr<ASTNode>> methods)
        : name(name), parent(parent), methods(move(methods)) {}

    void accept (Visitor& v) override { v.visit(*this); }
    void print(int indent) const override {
        cout << string(indent, ' ') << "Protocol(" << name << ")\n";
        if (!parent.empty())
            cout << string(indent + 2, ' ') << "Extends(" << parent << ")\n";
        for (const auto& method : methods)
            method->print(indent + 2);
    }
};

class RecursiveDescentParser {
private:
    const vector<pair<string,string>>& tokens;  // {tipo, lexeme}
    size_t cursor;

    const pair<string,string>& current() const { return tokens[cursor]; }
    const pair<string,string>& advance()       { return tokens[cursor++]; }

    bool check(const string& type) const { return current().first == type; }

    bool checkLexeme(const string& lexeme) const { return current().second == lexeme; }

    const pair<string,string>& expect(const string& type) {
        if (!check(type))
            throw runtime_error("Esperado tipo '" + type + "', encontrado '" + current().second + "'");
        return advance();
    }

    const pair<string,string>& expectLexeme(const string& lexeme) {
        if (!checkLexeme(lexeme))
            throw runtime_error("Esperado '" + lexeme + "', encontrado '" + current().second + "'");
        return advance();
    }

    // (name [: Type], name [: Type], ...) [Type]  -- consume los paréntesis
    pair<vector<pair<string, string>>, string> parseParams(bool types_required = false) {
        vector<pair<string, string>> params;
        expectLexeme("(");

        while (!checkLexeme(")")) {
            string pname = expect("id").second;
            string ptype;
            if (checkLexeme(":")) {
                advance();
                ptype = parseTypeAnnotation();
            } else if (types_required) {
                throw runtime_error("Parametro '" + pname + "' requiere anotacion de tipo");
            }
            params.push_back({pname, ptype});

            if (!checkLexeme(")"))
                expectLexeme(",");
        }
        expectLexeme(")");

        string return_type;
        if (checkLexeme(":")) {
            advance();
            return_type = parseTypeAnnotation();
        }

        return {params, return_type};
    }

    string parseTypeAnnotation() {
        string base;

        if (checkLexeme("(")) {
            // tipo función: (T, T') -> T''
            advance();
            base = "(";
            if (!checkLexeme(")")) {
                base += parseTypeAnnotation();
                while (checkLexeme(",")) {
                    advance();
                    base += ", " + parseTypeAnnotation();
                }
            }
            base += ")";
            expectLexeme(")");
            expectLexeme("->");
            base += " -> " + parseTypeAnnotation();
        } else {
            // tipo simple
            base = expect("id").second;
        }

        // sufijos: T* o T[]
        while (checkLexeme("*") || checkLexeme("[")) {
            if (checkLexeme("*")) {
                advance();
                base += "*";
            } else {
                advance();
                expectLexeme("]");
                base += "[]";
            }
        }

        return base;
    }


public:
    RecursiveDescentParser(const vector<pair<string,string>>& tokens)
        : tokens(tokens), cursor(0) {}


    unique_ptr<ASTNode> parse() {
        try
        {
            vector<unique_ptr<ASTNode>> declarations;

            //Parsea las declaraciones primero.
            while (checkLexeme("function") || checkLexeme("type") || checkLexeme("protocol") || checkLexeme("define")) {
                if (checkLexeme("function"))   declarations.push_back(parseFunction(false));
                if (checkLexeme("define")) declarations.push_back(parseFunction(true));
                if (checkLexeme("type"))     declarations.push_back(parseType());
                if (checkLexeme("protocol")) declarations.push_back(parseProtocol());
            }

            //Parsea la expresión principal.
            auto body = parseExpr();
            if(checkLexeme(";")) advance();


            expect("EOF");
            return make_unique<ProgramNode>(move(declarations), move(body));
        }
        catch (const runtime_error& e) {
            cerr << "(0,0) SYNTACTIC: " << e.what() << "\n";
            exit(2);
        }
    }

    // protocol Name [extends Parent] { method_signature; ... }
    unique_ptr<ASTNode> parseProtocol() {
        expectLexeme("protocol");
        string name = expect("id").second;

        string parent;
        if (checkLexeme("extends")) {
            advance();
            parent = expect("id").second;
        }

        expectLexeme("{");
        vector<unique_ptr<ASTNode>> methods;

        while (!checkLexeme("}")) {
            string method_name = expect("id").second;
            auto result = parseParams(true);  // tipos obligatorios

            vector<pair<string, string>> params = result.first;
            string return_type = result.second;

            if(return_type == "") {
                throw runtime_error("Método '" + method_name + "' requiere tipo de retorno");
            }

            expectLexeme(";");
            methods.push_back(make_unique<FunctionNode>(
                method_name, move(params), return_type, nullptr
            ));
        }

        expectLexeme("}");
        return make_unique<ProtocolNode>(name, parent, move(methods));
    }

    // type Name [(params)] [inherits Parent [(args)]] { members }
    unique_ptr<ASTNode> parseType() {
        expectLexeme("type");
        string name = expect("id").second;

        // params propios opcionales: type Point(x, y)
        vector<pair<string, string>> params;
        if (checkLexeme("(")) {
            auto result = parseParams();

            params = result.first;

            if(result.second != "") {
                throw runtime_error("Tipo '" + name + "' no puede tener tipo de retorno");
            }
        }

        // herencia opcional: inherits Parent [(args)]
        string parent;
        vector<unique_ptr<ASTNode>> parent_args;
        if (checkLexeme("inherits")) {
            advance();
            parent = expect("id").second;

            // args para el constructor del padre: opcionales
            if (checkLexeme("(")) {
                advance();
                if (!checkLexeme(")")) {
                    parent_args.push_back(parseExpr());
                    while (checkLexeme(",")) {
                        advance();
                        parent_args.push_back(parseExpr());
                    }
                }
                expectLexeme(")");
            }
        }

        // cuerpo
        expectLexeme("{");
        vector<tuple<string, string, unique_ptr<ASTNode>>> attributes;
        vector<unique_ptr<ASTNode>> methods;

        while (!checkLexeme("}")) {
            string member_name = expect("id").second;

            if (checkLexeme("(")) {
                // método
                auto result = parseParams();

                vector<pair<string, string>> method_params = result.first;
                string return_type = result.second;

                unique_ptr<ASTNode> body;
                if (checkLexeme("=>")) {
                    advance();
                    body = parseExpr();
                    expectLexeme(";");
                } else {
                    body = parseBlock();
                }
                methods.push_back(make_unique<FunctionNode>(member_name, move(method_params), return_type, move(body)));
            } else {
                // atributo
                string attr_type;
                if (checkLexeme(":")) {
                    advance();
                    attr_type = parseTypeAnnotation();
                }

                expectLexeme("=");
                auto value = parseExpr();
                expectLexeme(";");
                attributes.push_back({member_name, attr_type, move(value)});
            }
        }

        expectLexeme("}");
        return make_unique<TypeNode>(name, move(params), parent, move(parent_args), move(attributes), move(methods));
    }

    // function name(params) => expr   |   function name(params) { block }
    unique_ptr<ASTNode> parseFunction(bool is_macro) {
        if(!is_macro) expectLexeme("function");
        else expectLexeme("define");

        string name = expect("id").second;

        auto result = parseParams();

        vector<pair<string, string>> params = result.first;
        string return_type = result.second;


        // dos formas de cuerpo válidas
        unique_ptr<ASTNode> body;
        if (checkLexeme("=>") || checkLexeme("->")) {
            advance();
            body = parseExpr();
            expectLexeme(";");
        } else {
            body = parseBlock();  // parseBlock ya consume { y }
        }

        return make_unique<FunctionNode>(name, move(params), return_type, move(body), is_macro);
    }

    // [function] ( [params] )[: return_type] => | -> expr
    // [function] ( [params] )[: return_type] { block }
    unique_ptr<ASTNode> parseLambda() {
        if (checkLexeme("function")) {
            advance();
        }

        auto result = parseParams();

        vector<pair<string, string>> params = result.first;
        string return_type = result.second;

        unique_ptr<ASTNode> body;
        if (checkLexeme("=>") || checkLexeme("->")) {
            advance();
            body = parseExpr();
        } else {
            body = parseBlock();
        }

        return make_unique<FunctionNode>("", move(params), return_type, move(body));
    }


    // Expression -> let | for | Expr ...
    unique_ptr<ASTNode> parseExpr() {
        if (checkLexeme("let"))  return parseLetIn();
        if (checkLexeme("while")) return parseWhile();
        if (checkLexeme("for"))   return parseFor();
        if (checkLexeme("{"))     return parseBlock();

        auto left = parseCheckExpr();

        if (checkLexeme(":=")) {
            advance();
            auto* id = dynamic_cast<IdentifierNode*>(left.get());
            auto* member = dynamic_cast<MemberAccessNode*>(left.get());
            if (!id && !member)
                throw runtime_error("Lado izquierdo de := debe ser una variable o atributo");
            auto value = parseExpr();
            return make_unique<ReassignNode>(move(left), move(value));
        }

        return left;
    }

    // while (cond) body
    unique_ptr<ASTNode> parseWhile() {
        expectLexeme("while");
        expectLexeme("(");
        auto condition = parseExpr();
        expectLexeme(")");
        auto body = parseExpr();
        return make_unique<WhileNode>(move(condition), move(body));
    }

    // for (x in iterable) body
    unique_ptr<ASTNode> parseFor() {
        expectLexeme("for");
        expectLexeme("(");
        string var = expect("id").second;
        expectLexeme("in");
        auto iterable = parseExpr();
        expectLexeme(")");
        auto body = parseExpr();
        return make_unique<ForNode>(var, move(iterable), move(body));
    }

    // { expr; expr; expr; }
    unique_ptr<ASTNode> parseBlock() {
        expectLexeme("{");
        vector<unique_ptr<ASTNode>> expressions;

        while (!checkLexeme("}")) {
            expressions.push_back(parseExpr());
            expectLexeme(";");
        }

        expectLexeme("}");
        return make_unique<BlockNode>(move(expressions));
    }

    // let x = expr, y = expr in body
    unique_ptr<ASTNode> parseLetIn() {
        expectLexeme("let");

        vector<tuple<string, string, unique_ptr<ASTNode>>> bindings;

        do {
            string name = expect("id").second;
            string type;
            if (checkLexeme(":")) {
                advance();
                type = parseTypeAnnotation();
            }
            expectLexeme("=");
            bindings.push_back({name, type, move(parseExpr())});

            if (!checkLexeme(",")) break;
            advance();
        } while (true);

        expectLexeme("in");
        auto body = parseExpr();

        return make_unique<LetInNode>(move(bindings), move(body));
    }

    // if (cond) body elif (cond) body else body
    unique_ptr<ASTNode> parseIf() {
        expectLexeme("if");

        vector<pair<unique_ptr<ASTNode>, unique_ptr<ASTNode>>> branches;

        // rama if principal
        expectLexeme("(");
        auto cond = parseExpr();
        expectLexeme(")");
        auto body = parseExpr();
        branches.push_back({move(cond), move(body)});

        // ramas elif
        while (checkLexeme("elif")) {
            advance();
            expectLexeme("(");
            auto elif_cond = parseExpr();
            expectLexeme(")");
            auto elif_body = parseExpr();
            branches.push_back({move(elif_cond), move(elif_body)});
        }

        // else obligatorio en HULK
        expectLexeme("else");
        auto else_body = parseExpr();

        return make_unique<IfNode>(move(branches), move(else_body));
    }

    // CheckExpr -> OrExpr ("is" Type)?
    unique_ptr<ASTNode> parseCheckExpr() {
        auto left = parseOrExpr();
        if (checkLexeme("is")) {
            advance();
            string type_name = parseTypeAnnotation();
            return make_unique<IsNode>(move(left), type_name);
        }
        return left;
    }

    // OrExpr -> AndExpr ("|" AndExpr)*
    unique_ptr<ASTNode> parseOrExpr() {
        auto left = parseAndExpr();
        while (checkLexeme("|")) {
            string op = advance().second;
            auto right = parseAndExpr();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // AndExpr -> EqualityExpr ("&" EqualityExpr)*
    unique_ptr<ASTNode> parseAndExpr() {
        auto left = parseEqualityExpr();
        while (checkLexeme("&")) {
            string op = advance().second;
            auto right = parseEqualityExpr();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // EqualityExpr -> CompareExpr (("==" | "!=") CompareExpr)*
    unique_ptr<ASTNode> parseEqualityExpr() {
        auto left = parseCompareExpr();
        while (checkLexeme("==") || checkLexeme("!=")) {
            string op = advance().second;
            auto right = parseCompareExpr();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // CompareExpr -> ConcatExpr ((">" | "<" | ">=" | "<=") ConcatExpr)*
    unique_ptr<ASTNode> parseCompareExpr() {
        auto left = parseConcatExpr();
        while (checkLexeme(">") || checkLexeme("<") ||
               checkLexeme(">=") || checkLexeme("<=")) {
            string op = advance().second;
            auto right = parseConcatExpr();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // ConcatExpr -> ArithExpr (("@" | "@@") ArithExpr)*
    unique_ptr<ASTNode> parseConcatExpr() {
        auto left = parseArithExpr();
        while (checkLexeme("@") || checkLexeme("@@")) {
            string op = advance().second;
            auto right = parseArithExpr();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // ArithExpr -> Term (("+" | "-") Term)*
    unique_ptr<ASTNode> parseArithExpr() {
        auto left = parseTerm();
        while (checkLexeme("+") || checkLexeme("-")) {
            string op = advance().second;
            auto right = parseTerm();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // Term -> Pot (("*" | "/") Pot)*
    unique_ptr<ASTNode> parseTerm() {
        auto left = parsePot();
        while (checkLexeme("*") || checkLexeme("/")) {
            string op = advance().second;
            auto right = parsePot();
            left = make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // Pot -> UnaryExpr ("^" UnaryExpr)*  -- ojo: ^ es right-associative en HULK
    unique_ptr<ASTNode> parsePot() {
        auto left = parseUnaryExpr();
        if (checkLexeme("^") || checkLexeme("%")) {
            string op = advance().second;
            auto right = parsePot();  // recursión, no while — para asociatividad derecha
            return make_unique<BinaryOpNode>(op, move(left), move(right));
        }
        return left;
    }

    // UnaryExpr -> ("-" | "!") UnaryExpr | Factor
    unique_ptr<ASTNode> parseUnaryExpr() {
        if (!check("string") && (checkLexeme("-") || checkLexeme("!"))) {
            string op = advance().second;
            auto operand = parseUnaryExpr();  // recursión para "!!x", "--x"
            return make_unique<UnaryOpNode>(op, move(operand));
        }
        return parseFactor();
    }

    // Factor -> "(" Expr ")" | number | true | false | id | ...
    unique_ptr<ASTNode> parseFactor() {

        unique_ptr<ASTNode> node;

        if (checkLexeme("if"))   return parseIf();

        if (checkLexeme("(")) {
            advance();
            node = parseExpr();
            expectLexeme(")");
        }
        else if (check("number")) {
            node = make_unique<NumberNode>(stod(advance().second));
        }
        else if (checkLexeme("true"))  { advance(); node = make_unique<BoolNode>(true);  }
        else if (checkLexeme("false")) { advance(); node = make_unique<BoolNode>(false); }
        else if (check("string")) {
            node = make_unique<StringNode>(advance().second);
        }
        else if (check("id")) {
            node = make_unique<IdentifierNode>(advance().second);

                // ── Sufijos encadenados ──
            while (checkLexeme("(") || checkLexeme(".") || checkLexeme("[")) {
                if (checkLexeme("(")) {
                    advance();
                    vector<unique_ptr<ASTNode>> args;
                    if (!checkLexeme(")")) {
                        args.push_back(parseExpr());
                        while (checkLexeme(",")) {
                            advance();
                            args.push_back(parseExpr());
                        }
                    }
                    expectLexeme(")");
                    node = make_unique<CallNode>(move(node), move(args));
                }
                else if (checkLexeme(".")) {
                    advance();
                    string member = expect("id").second;
                    node = make_unique<MemberAccessNode>(move(node), member);
                }
                else if (checkLexeme("[")) {
                    advance();
                    auto index = parseExpr();
                    expectLexeme("]");
                    node = make_unique<IndexNode>(move(node), move(index));
                }
            }
        }
        else if (checkLexeme("new")) {
            advance();
            string type_name = expect("id").second; //Instancia de la clase

            auto callee = make_unique<IdentifierNode>(type_name);
            vector<unique_ptr<ASTNode>> args;

            expectLexeme("(");
            if (!checkLexeme(")")) {
                args.push_back(parseExpr());
                while (checkLexeme(",")) {
                    advance();
                    args.push_back(parseExpr());
                }
            }
            expectLexeme(")");


            node = make_unique<CallNode>(move(callee), move(args));
        }
        else if (checkLexeme("[")) {
            advance();

            // vector vacío
            if (checkLexeme("]")) {
                advance();
                node = make_unique<VectorNode>(vector<unique_ptr<ASTNode>>{});
            } else {
                // parseamos la primera expresión a partir de and para no consumir | y luego decidimos
                auto first = parseAndExpr();

                if (checkLexeme("|")) {
                    // es un generador: [expr | var in iterable]
                    advance();
                    string var = expect("id").second;
                    expectLexeme("in");
                    auto iterable = parseExpr();
                    expectLexeme("]");
                    node = make_unique<VectorGeneratorNode>(move(first), var, move(iterable));
                } else {
                    // es un literal: [expr, expr, ...]
                    vector<unique_ptr<ASTNode>> elements;
                    elements.push_back(move(first));
                    while (checkLexeme(",")) {
                        advance();
                        elements.push_back(parseExpr());
                    }
                    expectLexeme("]");
                    node = make_unique<VectorNode>(move(elements));
                }
            }
        }
        else if (checkLexeme("function")) {
            node = parseLambda();
        }
        else {
            throw runtime_error("Token inesperado: '" + current().second + "'");
        }


        return node;
    }
};
