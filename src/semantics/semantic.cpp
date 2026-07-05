using namespace std;

// ─── Tipos del sistema ────────────────────────────────────────────────────────

struct TypeInfo {
    string name;
    string parent;                                          // "" si es Object
    vector<pair<string,string>> attributes;                 // [(name, type)]
    vector<tuple<string, vector<string>, string>> methods;  // (name, param_types, return_type)
    vector<pair<string,string>> params;
    bool is_protocol = false;
};

struct FunctionInfo {
    string name;
    vector<pair<string,string>> params;               // [(name, type)]
    string return_type;
    bool is_macro;
};

struct VariableInfo {
    string name;
    string type;                                      // "" si no inferido aún
};


// ─── Tabla de símbolos ────────────────────────────────────────────────────────

class SymbolTable {
private:
    // scopes apilados — el último es el más interno
    vector<unordered_map<string, VariableInfo>> scopes;
    unordered_map<string, FunctionInfo> functions;
    unordered_map<string, TypeInfo> types;

public:
    SymbolTable() {
        pushScope();  // scope global
        registerBuiltins();
    }

    // ── Scopes ──
    void pushScope() {
        scopes.push_back({});
    }

    void popScope() {
        if (scopes.size() > 1)
            scopes.pop_back();
    }

    const unordered_map<string, TypeInfo>& getTypes() const { return types; }

    // ── Variables ──
    void declareVariable(const string& name, const string& type = "") {
        scopes.back()[name] = {name, type};
    }

    VariableInfo* lookupVariable(const string& name) {
        // busca de adentro hacia afuera
        for (int i = scopes.size() - 1; i >= 0; --i) {
            auto it = scopes[i].find(name);
            if (it != scopes[i].end())
                return &it->second;
        }
        return nullptr;
    }

    // ── Funciones ──
    void declareFunction(const FunctionInfo& info) {
        functions[info.name] = info;
    }

    FunctionInfo* lookupFunction(const string& name) {
        auto it = functions.find(name);
        return it != functions.end() ? &it->second : nullptr;
    }

    // ── Tipos ──
    void declareType(const TypeInfo& info) {
        types[info.name] = info;
    }

    TypeInfo* lookupType(const string& name) {
        auto it = types.find(name);
        return it != types.end() ? &it->second : nullptr;
    }

    // ── Builtins ──
    void registerBuiltins() {
        types["Iterable"] = {"Iterable", "", {}, {{"next", {}, "Boolean"}, {"current", {}, "Object"}}, {}, true};
        types["Vector"]   = {"Vector", "Iterable", {}, {{"size", {}, "Number"}}, {}, true};

        // funciones built-in de HULK
        functions["print"]  = {"print",  {{"value", "Object"}}, "Object", false};
        functions["sqrt"]   = {"sqrt",   {{"x", "Number"}},     "Number", false};
        functions["sin"]    = {"sin",    {{"x", "Number"}},     "Number", false};
        functions["cos"]    = {"cos",    {{"x", "Number"}},     "Number", false};
        functions["exp"]    = {"exp",    {{"x", "Number"}},     "Number", false};
        functions["rand"]   = {"rand",   {},                    "Number", false};
        functions["log"]    = {"log",    {{"x", "Number"}, {"y", "Number"}},     "Number", false};
        functions["range"]  = {"range",  {{"min","Number"},{"max","Number"}}, "Number*", false};

        // tipos built-in
        types["Object"]  = {"Object",  "", {}, {}};
        types["Number"]  = {"Number",  "Object", {}, {}};
        types["Boolean"] = {"Boolean", "Object", {}, {}};
        types["String"]  = {"String",  "Object", {}, {}};
    }
};

// ─── SemanticAnalyzer ─────────────────────────────────────────────────────────

class SemanticAnalyzer : public Visitor {
private:
    SymbolTable table;
    string last_type;  // tipo de la última expresión visitada
    vector<string> errors;

    void error(const string& msg) {
        errors.push_back(msg);
    }

    // Visita un nodo y devuelve su tipo inferido
    string infer(ASTNode& node) {
        node.accept(*this);
        node.inferred_type = last_type;
        return last_type;
    }

    // ── Registro previo de declaraciones ──
    void registerDeclaration(ASTNode& node) {
        if (auto* fn = dynamic_cast<FunctionNode*>(&node)) {
            if (table.lookupFunction(fn->name))
                error("Función '" + fn->name + "' ya está declarada");
            else
                table.declareFunction({fn->name, fn->params, fn->return_type, fn->is_macro});
        }
        else if (auto* tn = dynamic_cast<TypeNode*>(&node)) {
            if (table.lookupType(tn->name))
                error("Tipo '" + tn->name + "' ya está declarado");
            else {
                TypeInfo info;
                info.name   = tn->name;
                info.parent = tn->parent.empty() ? "Object" : tn->parent;
                info.params = tn->params;
                for (const auto& [name, type, val] : tn->attributes)
                    info.attributes.push_back({name, type});
                for (const auto& method : tn->methods) {
                    if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                        vector<string> param_types;
                        for (const auto& [pname, ptype] : fn->params)
                            param_types.push_back(ptype);
                        info.methods.push_back({fn->name, param_types, fn->return_type});
                    }
                }
                table.declareType(info);
            }
        }
        else if (auto* pn = dynamic_cast<ProtocolNode*>(&node)) {
            if (table.lookupType(pn->name))
                error("Protocolo '" + pn->name + "' ya está declarado");
            else {
                TypeInfo info;
                info.name   = pn->name;
                info.parent = pn->parent.empty() ? "Object" : pn->parent;
                info.is_protocol = true;
                for (const auto& method : pn->methods) {
                    if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                        vector<string> param_types;
                        for (const auto& [pname, ptype] : fn->params)
                            param_types.push_back(ptype);
                        info.methods.push_back({fn->name, param_types, fn->return_type});
                    }
                }
                table.declareType(info);
            }
        }
    }

    //Helpers para actualizar y verificar la tabla de tipos
    void analyzeMethod(FunctionNode* fn, const string& type_name) {
        table.pushScope();
        for (const auto& [pname, ptype] : fn->params)
            table.declareVariable(pname, ptype);
        if (fn->body)
            infer(*fn->body);
        table.popScope();
    }

    void updateMethodReturnTypes(TypeNode& node) {
        TypeInfo* info = table.lookupType(node.name);
        if (!info) return;
        for (auto& method : node.methods) {
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                for (auto& [mname, param_types, return_type] : info->methods) {
                    if (mname == fn->name && return_type.empty() && fn->body)
                        return_type = fn->body->inferred_type;
                }
            }
        }
    }

    void verifyMethod(FunctionNode* fn, const string& type_name) {
        table.pushScope();
        for (const auto& [pname, ptype] : fn->params)
            table.declareVariable(pname, ptype);
        if (fn->body) {
            string body_type = infer(*fn->body);
            if (!fn->return_type.empty() && !isCompatible(body_type, fn->return_type))
                error("Metodo '" + fn->name + "' de tipo '" + type_name +
                      "' declarado con retorno '" + fn->return_type +
                      "' pero devuelve '" + body_type + "'");
        }
        table.popScope();
    }

    // Verifica que type_name tiene todos los métodos requeridos por protocol_name
    bool satisfies(const string& type_name, const string& protocol_name) {
        TypeInfo* protocol = table.lookupType(protocol_name);
        TypeInfo* type     = table.lookupType(type_name);
        if (!protocol || !type) return false;

        unordered_set<string> required = collectProtocolMethods(protocol_name);

        // recoger métodos del tipo incluyendo heredados
        unordered_map<string, vector<string>> type_methods;
        string current = type_name;
        while (!current.empty()) {
            TypeInfo* info = table.lookupType(current);
            if (!info) break;
            for (const auto& [method_name, param_types, return_type] : info->methods)
                if (!type_methods.count(method_name))
                    type_methods[method_name] = param_types;
            current = info->parent;
        }

        for (const auto& method_name : required)
            if (!type_methods.count(method_name))
                return false;

        return true;
    }

    // Punto de entrada unificado para verificar compatibilidad de tipos
    bool isCompatible(const string& actual, const string& expected) {
        if (actual == expected) return true;
        if (actual.empty() || expected.empty()) return true;

        // ── Azúcar sintáctica de iterables ──

        // T[] es compatible con Vector y con Iterable
        if (actual.size() >= 2 && actual.substr(actual.size() - 2) == "[]") {
            if (expected == "Vector" || expected == "Iterable") return true;
            // T[] también es compatible con T*
            string actual_base   = actual.substr(0, actual.size() - 2);
            if (expected.size() > 1 && expected.back() == '*') {
                string expected_base = expected.substr(0, expected.size() - 1);
                return isCompatible(actual_base, expected_base);
            }
        }

        // T* es compatible con Iterable
        if (actual.size() > 1 && actual.back() == '*') {
            if (expected == "Iterable") return true;
            // T* es compatible con U* si T es compatible con U
            if (expected.size() > 1 && expected.back() == '*') {
                string actual_base   = actual.substr(0, actual.size() - 1);
                string expected_base = expected.substr(0, expected.size() - 1);
                return isCompatible(actual_base, expected_base);
            }
        }

        // functor: "(T) -> T'"
        if (expected.front() == '(') {
            TypeInfo* info = table.lookupType(actual);
            if (!info) return false;
            for (const auto& [method_name, param_types, return_type] : info->methods)
                if (method_name == "invoke") return true;
            return false;
        }

        TypeInfo* expected_info = table.lookupType(expected);

        // protocolo — verificación estructural
        if (expected_info && expected_info->is_protocol)
            return satisfies(actual, expected);

        // herencia nominal
        return lca(actual, expected) == expected;
    }

    // Busca el ancestro común más bajo entre dos tipos. Usado en IfNode y CallNode
    string lca(const string& a, const string& b) {
        if (a == b) return a;
        if (a == "" || b == "") return "";

        // si b es protocolo y a lo satisface, el lca es b
        TypeInfo* b_info = table.lookupType(b);
        if (b_info && b_info->is_protocol && satisfies(a, b)) return b;

        TypeInfo* a_info = table.lookupType(a);
        if (a_info && a_info->is_protocol && satisfies(b, a)) return a;

        // construye la cadena de ancestros de 'a'
        unordered_set<string> ancestors_a;
        string current = a;
        while (!current.empty()) {
            ancestors_a.insert(current);
            auto* info = table.lookupType(current);
            if (!info || info->parent.empty()) break;
            current = info->parent;
        }
        ancestors_a.insert("Object");

        // sube por la cadena de 'b' hasta encontrar uno en ancestors_a
        current = b;
        while (!current.empty()) {
            if (ancestors_a.count(current)) return current;
            auto* info = table.lookupType(current);
            if (!info || info->parent.empty()) break;
            current = info->parent;
        }

        return "Object";  // fallback — siempre existe
    }

    // Recoge todos los nombres de métodos de un protocolo y sus ancestros
    unordered_set<string> collectProtocolMethods(const string& protocol_name) {
        unordered_set<string> methods;
        string current = protocol_name;

        while (!current.empty()) {
            TypeInfo* info = table.lookupType(current);
            if (!info) break;

            for (const auto& [method_name, param_types, return_type] : info->methods)
                methods.insert(method_name);

            current = info->parent;
        }

        return methods;
    }

    // Extrae el tipo del elemento de un iterable — T* -> T, T[] -> T
    string resolveIterableElement(const string& type) {
        if (type.size() > 1 && type.back() == '*')
            return type.substr(0, type.size() - 1);
        if (type.size() >= 2 && type.substr(type.size() - 2) == "[]")
            return type.substr(0, type.size() - 2);
        if (!type.empty() && satisfies(type, "Iterable")) {
            // buscar el tipo de retorno real de current() en el tipo concreto
            string current = type;
            while (!current.empty()) {
                TypeInfo* info = table.lookupType(current);
                if (!info) break;
                for (const auto& [mname, param_types, return_type] : info->methods)
                    if (mname == "current" && !return_type.empty())
                        return return_type;
                current = info->parent;
            }
            return "Object";  // fallback si no encuentra current()
        }
        return "";
    }

public:
    bool analyze(ProgramNode& program) {
        program.accept(*this);
        if (!errors.empty()) {
            for (const auto& e : errors)
                cerr << "(0,0) SEMANTIC: " << e << "\n";
            return false;
        }
        return true;
    }

    const vector<string>& getErrors() const { return errors; }

    // ── Literales ── tipos triviales, no hay nada que verificar
    void visit(NumberNode&)  override { last_type = "Number";  }
    void visit(BoolNode&)    override { last_type = "Boolean"; }
    void visit(StringNode&)  override { last_type = "String";  }

    // ── Identificador ── busca en la tabla de símbolos
    void visit(IdentifierNode& node) override {
        auto* var = table.lookupVariable(node.name);
        if (!var) {
            error("Variable no declarada: '" + node.name + "'");
            last_type = "";
            return;
        }
        last_type = var->type;
    }

    // ── Programa ── dos pasadas: primero declara todo, luego analiza el body
    void visit(ProgramNode& node) override {
        // primera pasada: registrar funciones y tipos para que puedan referenciarse entre sí independientemente del orden
        for (auto& decl : node.declarations)
            registerDeclaration(*decl);

        // segunda pasada: analizar los cuerpos
        for (auto& decl : node.declarations)
            infer(*decl);

        // expresión principal
        infer(*node.body);
    }

    void visit(IsNode& node) override {
        string expr_type = infer(*node.expr);

        if (!table.lookupType(node.type_name))
            error("Tipo no declarado: '" + node.type_name + "'");

        // is requiere tipo conocido en compile time
        if (expr_type.empty() || expr_type == "Object")
            error("'is' requiere que el tipo de la expresion sea conocido en compile time — si es un parámetro, se recomienda anotar su tipo");

        // si expr_type nunca puede ser type_name, el 'is' siempre será false
        TypeInfo* info = table.lookupType(node.type_name);
        if (info && !expr_type.empty()) {
            if (info->is_protocol && !satisfies(expr_type, node.type_name))
                error("Tipo '" + expr_type + "' nunca satisfará protocolo '" + node.type_name + "'");
            else if (!info->is_protocol && !isCompatible(expr_type, node.type_name)
                                        && !isCompatible(node.type_name, expr_type))
                error("Tipo '" + expr_type + "' nunca será '" + node.type_name + "'");
        }

        last_type = "Boolean";
    }

    void visit(ReassignNode& node) override {
        string target_type = infer(*node.target);
        string value_type  = infer(*node.value);

        if (!dynamic_cast<IdentifierNode*>(node.target.get()) &&
            !dynamic_cast<MemberAccessNode*>(node.target.get()) &&
            !dynamic_cast<IndexNode*>(node.target.get()))
            error("El lado izquierdo de ':=' debe ser una variable, atributo o índice");

        if (auto* ma = dynamic_cast<MemberAccessNode*>(node.target.get())) {
            if (auto* id = dynamic_cast<IdentifierNode*>(ma->object.get())) {
                if (id->name != "self")
                    error("Solo se puede reasignar atributos propios via 'self'");
            }
        }

        if (!isCompatible(value_type, target_type))
            error("No se puede asignar '" + value_type +
                  "' a variable de tipo '" + target_type + "'");

        last_type = value_type;
    }

    void visit(UnaryOpNode& node) override {
        string operand_type = infer(*node.operand);

        if (node.op == "-") {
            if (operand_type != "Number" && operand_type != "")
                error("Operador '-' requiere Number, encontrado '" + operand_type + "'");
            last_type = "Number";
        }
        else if (node.op == "!") {
            if (operand_type != "Boolean" && operand_type != "")
                error("Operador '!' requiere Boolean, encontrado '" + operand_type + "'");
            last_type = "Boolean";
        }
    }

    void visit(BinaryOpNode& node) override {
        string left_type  = infer(*node.left);
        string right_type = infer(*node.right);

        // ── Aritméticos: Number x Number -> Number ──
        if (node.op == "+" || node.op == "-" ||
            node.op == "*" || node.op == "/" ||
            node.op == "^" || node.op == "%") {
            if (left_type  != "Number" && left_type  != "")
                error("Operador '" + node.op + "' requiere Number en lado izquierdo, encontrado '" + left_type + "'");
            if (right_type != "Number" && right_type != "")
                error("Operador '" + node.op + "' requiere Number en lado derecho, encontrado '" + right_type + "'");
            last_type = "Number";
        }

        // ── Comparación: Number x Number -> Boolean ──
        else if (node.op == ">" || node.op == "<" ||
                 node.op == ">=" || node.op == "<=") {
            if (left_type  != "Number" && left_type  != "")
                error("Operador '" + node.op + "' requiere Number en lado izquierdo, encontrado '" + left_type + "'");
            if (right_type != "Number" && right_type != "")
                error("Operador '" + node.op + "' requiere Number en lado derecho, encontrado '" + right_type + "'");
            last_type = "Boolean";
        }

        // ── Igualdad: T x T -> Boolean ──
        else if (node.op == "==" || node.op == "!=") {
            if (left_type != right_type && left_type != "" && right_type != "")
                error("Operador '" + node.op + "' requiere tipos iguales, encontrado '" + left_type + "' y '" + right_type + "'");
            last_type = "Boolean";
        }

        // ── Lógicos: Boolean x Boolean -> Boolean ──
        else if (node.op == "|" || node.op == "&") {
            if (left_type  != "Boolean" && left_type  != "")
                error("Operador '" + node.op + "' requiere Boolean en lado izquierdo, encontrado '" + left_type + "'");
            if (right_type != "Boolean" && right_type != "")
                error("Operador '" + node.op + "' requiere Boolean en lado derecho, encontrado '" + right_type + "'");
            last_type = "Boolean";
        }

        // ── Concatenación: Object x Object -> String ──
        else if (node.op == "@" || node.op == "@@") {
            last_type = "String";  // cualquier tipo es válido, se convierte a string
        }
    }

    void visit(BlockNode& node) override {
        // el tipo del bloque es el tipo de la última expresión
        string result_type;
        for (auto& expr : node.expressions)
            result_type = infer(*expr);
        last_type = result_type;
    }

    void visit(WhileNode& node) override {
        string cond_type = infer(*node.condition);
        if (cond_type != "Boolean" && cond_type != "")
            error("Condicion de 'while' debe ser Boolean, encontrado '" + cond_type + "'");

        last_type = infer(*node.body);  // el tipo es el del cuerpo
    }

    void visit(IfNode& node) override {
        string result_type;

        for (auto& [cond, body] : node.branches) {
            string cond_type = infer(*cond);
            if (cond_type != "Boolean" && cond_type != "")
                error("Condicion de 'if' debe ser Boolean, encontrado '" + cond_type + "'");

            string body_type = infer(*body);
            result_type = result_type.empty() ? body_type : lca(result_type, body_type);
        }

        string else_type = infer(*node.else_body);
        last_type = lca(result_type, else_type);
    }

    void visit(LetInNode& node) override {
        table.pushScope();

        for (auto& [name, type, value] : node.bindings) {
            string val_type = infer(*value);

            // si tiene anotación, verificar compatibilidad
            if (!type.empty() && !isCompatible(val_type, type))
                error("Variable '" + name + "' declarada como '" + type +
                      "' pero inicializada con '" + val_type + "'");

            // el tipo real es la anotación si existe, sino el inferido
            string actual_type = type.empty() ? val_type : type;
            table.declareVariable(name, actual_type);
        }

        last_type = infer(*node.body);

        table.popScope();
    }

    void visit(ForNode& node) override {
        string iterable_type = infer(*node.iterable);
        string element_type  = resolveIterableElement(iterable_type);

        if (element_type.empty() && !iterable_type.empty())
            error("'for' requiere un iterable, encontrado '" + iterable_type + "'");
        if (element_type.empty()) element_type = "Object";

        table.pushScope();
        table.declareVariable(node.variable, element_type);
        last_type = infer(*node.body);
        table.popScope();
    }

    void visit(FunctionNode& node) override {
        // si no tiene nombre es una lambda — no registra en la tabla
        // el registro de funciones con nombre ya ocurrió en la primera pasada

        table.pushScope();

        for (const auto& [pname, ptype] : node.params)
            table.declareVariable(pname, ptype);

        // si no tiene cuerpo es una declaración de protocolo
        if (!node.body) {
            table.popScope();
            last_type = node.return_type;
            return;
        }

        string body_type = infer(*node.body);

        // verificar que el tipo de retorno coincide con la anotación si existe
        if (!node.return_type.empty() && !isCompatible(body_type, node.return_type))
            error("Funcion '" + node.name + "' declarada con retorno '" + node.return_type +
                  "' pero devuelve '" + body_type + "'");

        table.popScope();

        // el tipo de la función es su tipo de retorno
        last_type = node.return_type.empty() ? body_type : node.return_type;
    }

    void visit(CallNode& node) override {
        vector<string> arg_types;
        for (auto& arg : node.args)
            arg_types.push_back(infer(*arg));

        // ── Caso 1: f(args) — función libre o constructor ──
        if (auto* id = dynamic_cast<IdentifierNode*>(node.callee.get())) {
            string callee_name = id->name;

            FunctionInfo* fn = table.lookupFunction(callee_name);
            TypeInfo* tn     = table.lookupType(callee_name);

            if (!fn && !tn) {
                error("Funcion o tipo no declarado: '" + callee_name + "'");
                last_type = "";
                return;
            }

            if (fn) {
                if (arg_types.size() != fn->params.size())
                    error("Funcion '" + callee_name + "' espera " +
                          to_string(fn->params.size()) + " argumentos, recibidos " +
                          to_string(arg_types.size()));

                for (size_t i = 0; i < min(arg_types.size(), fn->params.size()); ++i) {
                    const string& expected = fn->params[i].second;
                    const string& actual   = arg_types[i];
                    if (!expected.empty() && !actual.empty() && !isCompatible(actual, expected))
                        error("Argumento " + to_string(i+1) + " de '" + callee_name +
                              "' espera '" + expected + "', recibido '" + actual + "'");
                }
                last_type = fn->return_type;
            }
            else if (tn) {
                if (tn->is_protocol) {
                    error("No se puede instanciar el protocolo '" + callee_name + "'");
                    last_type = "";
                    return;
                }
                last_type = tn->name;
            }
        }

        // ── Caso 2: obj.method(args) — llamada a método ──
        else if (auto* ma = dynamic_cast<MemberAccessNode*>(node.callee.get())) {
            string object_type = infer(*ma->object);

            if (object_type.empty()) {
                last_type = "";
                return;
            }

            // resolver tipo base
            string base_type = object_type;
            if (object_type.size() > 2 && object_type.substr(object_type.size() - 2) == "[]") {
                base_type = "Vector";   // T[] — buscar métodos en Vector
            } else if (object_type.size() > 1 && object_type.back() == '*') {
                base_type = "Iterable"; // T* — buscar métodos en Iterable
            }


            // buscar el método subiendo por la jerarquía
            string current = base_type;
            bool found = false;
            while (!current.empty()) {
                TypeInfo* info = table.lookupType(current);
                if (!info) break;

                for (const auto& [method_name, param_types, return_type] : info->methods) {
                    if (method_name == ma->member) {
                        // verificar número de argumentos
                        if (arg_types.size() != param_types.size())
                            error("Metodo '" + ma->member + "' espera " +
                                  to_string(param_types.size()) + " argumentos, recibio " +
                                  to_string(arg_types.size()));

                        // verificar tipos de argumentos
                        for (size_t i = 0; i < min(arg_types.size(), param_types.size()); ++i) {
                            const string& expected = param_types[i];
                            const string& actual   = arg_types[i];
                            if (!expected.empty() && !actual.empty() && !isCompatible(actual, expected))
                                error("Argumento " + to_string(i+1) + " de metodo '" +
                                      ma->member + "' espera '" + expected +
                                      "', recibido '" + actual + "'");
                        }

                        last_type = return_type;
                        found = true;
                        break;
                    }
                }
                if (found) return;
                current = info->parent;
            }

            if (!found) {
                error("Metodo '" + ma->member + "' no encontrado en tipo '" + base_type + "'");
                last_type = "";
            }
        }

        // ── Caso 3: expr(args) — functor ──
        else {
            string callee_type = infer(*node.callee);
            if (!callee_type.empty() && callee_type.front() != '(')
                error("Expresion de tipo '" + callee_type + "' no es invocable");
            last_type = "";  // el tipo de retorno del functor requiere parsear callee_type
        }
}

    void visit(VectorNode& node) override {
        if (node.elements.empty()) {
            last_type = "Object[]";
            return;
        }

        string element_type = infer(*node.elements[0]);

        for (size_t i = 1; i < node.elements.size(); ++i) {
            string current_type = infer(*node.elements[i]);
            if (current_type != element_type && current_type != "" && element_type != "") {
                error("Vector con tipos incompatibles: '" + element_type + "' y '" + current_type + "'");
                last_type = "";
                return;
            }
            if (element_type == "" && current_type != "") {
                element_type = current_type;
            }
        }

        last_type = element_type + "[]";
    }

    void visit(VectorGeneratorNode& node) override {
        string iterable_type = infer(*node.iterable);
        string element_type  = resolveIterableElement(iterable_type);

        if (element_type.empty() && !iterable_type.empty())
            error("Generador requiere un iterable, encontrado '" + iterable_type + "'");
        if (element_type.empty()) element_type = "Object";

        table.pushScope();
        table.declareVariable(node.variable, element_type);
        string expr_type = infer(*node.expr);
        table.popScope();

        last_type = expr_type + "[]";
    }

    void visit(IndexNode& node) override {
        string object_type = infer(*node.object);
        string index_type = infer(*node.index);

        // verificar que el índice es Number
        if (index_type != "Number" && index_type != "")
            error("Índice de acceso debe ser Number, encontrado '" + index_type + "'");

        // extraer el tipo base del iterable
        string base_type = "";
        if (object_type.size() > 1 && object_type.back() == '*') {
            base_type = object_type.substr(0, object_type.size() - 1);
        } else if (object_type.size() > 2 &&
                   object_type.substr(object_type.size() - 2) == "[]") {
            base_type = object_type.substr(0, object_type.size() - 2);
        } else {
            error("No se puede indexar tipo '" + object_type + "'");
            last_type = "";
            return;
        }

        last_type = base_type;
    }

    void visit(ProtocolNode& node) override {
        if (!node.parent.empty() && !table.lookupType(node.parent))
            error("Protocolo '" + node.name + "' extiende protocolo no declarado: '" + node.parent + "'");

        // métodos heredados del padre
        unordered_set<string> inherited_methods;
        if (!node.parent.empty())
            inherited_methods = collectProtocolMethods(node.parent);

        // verificar métodos propios
        unordered_set<string> seen_methods;
        for (const auto& method : node.methods) {
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {

                // duplicado en el mismo protocolo
                if (seen_methods.count(fn->name))
                    error("Metodo duplicado '" + fn->name + "' en protocolo '" + node.name + "'");

                // override de método heredado
                if (inherited_methods.count(fn->name))
                    error("Protocolo '" + node.name + "' no puede redefinir metodo '" +
                          fn->name + "' heredado de '" + node.parent + "'");

                seen_methods.insert(fn->name);

                // verificar que los tipos en las firmas existen
                for (const auto& [pname, ptype] : fn->params) {
                    if (!ptype.empty() && !table.lookupType(ptype))
                        error("Tipo '" + ptype + "' en parámetro '" + pname +
                              "' del metodo '" + fn->name + "' no declarado");
                }
                if (!fn->return_type.empty() && !table.lookupType(fn->return_type))
                    error("Tipo de retorno '" + fn->return_type +
                          "' del metodo '" + fn->name + "' no declarado");
            }
        }

        last_type = "";
    }

    void visit(TypeNode& node) override {
        // ── 1. Verificar que el padre existe y no es protocolo ──
        if (!node.parent.empty()) {
            TypeInfo* parent_info = table.lookupType(node.parent);
            if (!parent_info)
                error("Tipo '" + node.name + "' hereda de tipo no declarado: '" + node.parent + "'");
            else if (parent_info->is_protocol)
                error("Tipo '" + node.name + "' no puede heredar de protocolo '" + node.parent + "'");
            else {
                // verificar herencia circular
                string current = node.parent;
                while (!current.empty()) {
                    if (current == node.name) {
                        error("Herencia circular detectada: '" + node.name + "' hereda de sí mismo");
                        break;
                    }
                    TypeInfo* info = table.lookupType(current);
                    if (!info) break;
                    current = info->parent;
                }
            }
        }

        // ── 2. Verificar duplicados ──
        unordered_set<string> seen;

        for (const auto& [attr_name, attr_type, val] : node.attributes) {
            if (seen.count(attr_name))
                error("Atributo duplicado '" + attr_name + "' en tipo '" + node.name + "'");
            seen.insert(attr_name);
        }

        for (const auto& method : node.methods) {
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                if (seen.count(fn->name))
                    error("Miembro duplicado '" + fn->name + "' en tipo '" + node.name + "'");
                seen.insert(fn->name);
            }
        }

        // ── 3. Verificar argumentos al constructor del padre ──
        if (!node.parent.empty() && !node.parent_args.empty()) {
            TypeInfo* parent_info = table.lookupType(node.parent);

            if (parent_info) {
                // los argumentos del padre se evalúan en el scope del tipo hijo con acceso a los parámetros del tipo hijo
                table.pushScope();
                for (const auto& [pname, ptype] : node.params)
                    table.declareVariable(pname, ptype);

                // verificar número de argumentos
                if (node.parent_args.size() != parent_info->params.size())
                    error("Tipo '" + node.name + "' pasa " +
                          to_string(node.parent_args.size()) + " argumentos a '" +
                          node.parent + "' que espera " +
                          to_string(parent_info->params.size()));

                // verificar tipos de argumentos
                for (size_t i = 0; i < min(node.parent_args.size(),
                                            parent_info->attributes.size()); ++i) {
                    string arg_type      = infer(*node.parent_args[i]);
                    const string& expected = parent_info->attributes[i].second;
                    if (!expected.empty() && !isCompatible(arg_type, expected))
                        error("Argumento " + to_string(i+1) + " al constructor de '" +
                              node.parent + "' espera '" + expected +
                              "', recibido '" + arg_type + "'");
                }

                table.popScope();
            }
        }

        // ── 4. Analizar atributos ──
        table.pushScope();

        // self es accesible en atributos y métodos
        table.declareVariable("self", node.name);

        // parámetros del tipo también son accesibles en atributos
        for (const auto& [pname, ptype] : node.params)
            table.declareVariable(pname, ptype);

        for (const auto& [attr_name, attr_type, val] : node.attributes) {
            string value_type = infer(*val);

            if (!attr_type.empty() && !isCompatible(value_type, attr_type))
                error("Atributo '" + attr_name + "' de tipo '" + node.name +
                      "' declarado como '" + attr_type +
                      "' pero inicializado con '" + value_type + "'");

            // registrar el atributo en el scope para que métodos posteriores puedan referenciarlo via self
            string actual_type = attr_type.empty() ? value_type : attr_type;
            table.declareVariable(attr_name, actual_type);
        }

        // ── 5. Analizar métodos ──
        for (const auto& method : node.methods)
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get()))
                analyzeMethod(fn, node.name);

        updateMethodReturnTypes(node);

        for (const auto& method : node.methods)
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get()))
                verifyMethod(fn, node.name);

        // ── anotar protocolos satisfechos ──
        for (const auto& [proto_name, proto_info] : table.getTypes()) {
            if (!proto_info.is_protocol) continue;
            if (!satisfies(node.name, proto_name)) continue;

            // no añadir si el padre ya lo satisface
            bool parent_satisfies = !node.parent.empty() && satisfies(node.parent, proto_name);
            if (!parent_satisfies)
                node.satisfied_protocols.push_back(proto_name);
        }

        // ── 6. Verificar compatibilidad con métodos heredados ──
        if (!node.parent.empty()) {
            TypeInfo* parent_info = table.lookupType(node.parent);

            if (parent_info) {
                // construir mapa de métodos del padre incluyendo sus ancestros
                unordered_map<string, tuple<vector<string>, string>> parent_methods;
                string current = node.parent;
                while (!current.empty()) {
                    TypeInfo* info = table.lookupType(current);
                    if (!info) break;
                    for (const auto& [mname, param_types, return_type] : info->methods)
                        if (!parent_methods.count(mname))
                            parent_methods[mname] = {param_types, return_type};
                    current = info->parent;
                }

                // verificar cada método del hijo que sobreescribe uno del padre
                for (const auto& method : node.methods) {
                    if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                        auto it = parent_methods.find(fn->name);
                        if (it == parent_methods.end()) continue;  // método nuevo, no override

                        const auto& [parent_param_types, parent_return_type] = it->second;

                        // verificar número de parámetros
                        if (fn->params.size() != parent_param_types.size())
                            error("Metodo '" + fn->name + "' en '" + node.name +
                                  "' sobreescribe al padre con distinto número de parámetros");

                        // verificar tipos de parámetros
                        for (size_t i = 0; i < min(fn->params.size(), parent_param_types.size()); ++i) {
                            const string& child_type  = fn->params[i].second;
                            const string& parent_type = parent_param_types[i];
                            if (!child_type.empty() && !parent_type.empty() &&
                                !isCompatible(child_type, parent_type))
                                error("Parámetro " + to_string(i+1) + " de metodo '" +
                                      fn->name + "' en '" + node.name +
                                      "' es incompatible con el padre: espera '" +
                                      parent_type + "', declarado '" + child_type + "'");
                        }

                        // verificar tipo de retorno
                        if (!fn->return_type.empty() && !parent_return_type.empty() &&
                            !isCompatible(fn->return_type, parent_return_type))
                            error("Metodo '" + fn->name + "' en '" + node.name +
                                  "' sobreescribe al padre con tipo de retorno incompatible: espera '" +
                                  parent_return_type + "', declarado '" + fn->return_type + "'");
                    }
                }
            }
        }

        // cerrar el scope de self y atributos
        table.popScope();


        last_type = "";
    }

    void visit(MemberAccessNode& node) override {
        string object_type = infer(*node.object);

        if (object_type.empty()) {
            last_type = "";
            return;
        }

        // resolver el tipo base
        string base_type = object_type;
        if (object_type.size() > 2 && object_type.substr(object_type.size() - 2) == "[]") {
            base_type = "Vector";
        } else if (object_type.size() > 1 && object_type.back() == '*') {
            base_type = "Iterable";
        }

        // si no tiene sufijo, buscar el tipo en la tabla
        TypeInfo* type_info = table.lookupType(base_type);
        if (!type_info) {
            error("Tipo '" + base_type + "' no encontrado para acceso a miembro '" + node.member + "'");
            last_type = "";
            return;
        }

        // buscar el miembro subiendo por la jerarquía de herencia
        string current = base_type;
        while (!current.empty()) {
            TypeInfo* info = table.lookupType(current);
            if (!info) break;

            // buscar en atributos
            for (const auto& [attr_name, attr_type] : info->attributes) {
                if (attr_name == node.member) {
                    last_type = attr_type;
                    return;
                }
            }

            // buscar en métodos
            for (const auto& [method_name, param_types, return_type] : info->methods) {
                if (method_name == node.member) {
                    last_type = return_type;
                    return;
                }
            }

            current = info->parent;
        }

        error("Miembro '" + node.member + "' no encontrado en tipo '" + base_type + "'");
        last_type = "";
    }

};


struct PrintVisitor : public Visitor {
    void visit(ProgramNode& n)         override { n.print(0); }
    void visit(NumberNode& n)          override { n.print(0); }
    void visit(BoolNode& n)            override { n.print(0); }
    void visit(StringNode& n)          override { n.print(0); }
    void visit(IdentifierNode& n)      override { n.print(0); }
    void visit(IsNode& n)              override { n.print(0); }
    void visit(BinaryOpNode& n)        override { n.print(0); }
    void visit(UnaryOpNode& n)         override { n.print(0); }
    void visit(ReassignNode& n)        override { n.print(0); }
    void visit(CallNode& n)            override { n.print(0); }
    void visit(MemberAccessNode& n)    override { n.print(0); }
    void visit(IndexNode& n)           override { n.print(0); }
    void visit(VectorNode& n)          override { n.print(0); }
    void visit(VectorGeneratorNode& n) override { n.print(0); }
    void visit(LetInNode& n)           override { n.print(0); }
    void visit(IfNode& n)              override { n.print(0); }
    void visit(WhileNode& n)           override { n.print(0); }
    void visit(ForNode& n)             override { n.print(0); }
    void visit(BlockNode& n)           override { n.print(0); }
    void visit(FunctionNode& n)        override { n.print(0); }
    void visit(TypeNode& n)            override { n.print(0); }
    void visit(ProtocolNode& n)        override { n.print(0); }
};
