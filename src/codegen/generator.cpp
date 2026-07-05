using namespace std;

#include <sstream>
#include <string>
#include <vector>
#include <memory>

#include "hulk_runtime.h"
#include "../semantics/visitor.h"


// ─── CodeGenerator ────────────────────────────────────────────────────────────

class CodeGenerator : public Visitor {
private:
    ostringstream out;      // código generado
    int indent_level = 0;   // nivel de indentación actual

    string indent() const {
        return string(indent_level * 4, ' ');
    }

    void emit(const string& code) {
        out << code;
    }

    void emitLine(const string& code = "") {
        out << indent() << code << "\n";
    }

    // Convierte un tipo HULK a su equivalente C++
    string toCppType(const string& hulk_type) {
        if (hulk_type.empty()) return "auto";
        if (hulk_type == "Object") return "HulkObject*";
        if (hulk_type == "Number")  return "Number";
        if (hulk_type == "Boolean") return "Boolean";
        if (hulk_type == "String")  return "String";

        // T[] → std::vector<T>
        if (hulk_type.size() >= 2 && hulk_type.substr(hulk_type.size() - 2) == "[]") {
            string base = hulk_type.substr(0, hulk_type.size() - 2);
            return "std::vector<" + toCppType(base) + ">";
        }

        // T* → std::vector<T>  (range devuelve vector, for lo itera)
        if (hulk_type.size() > 1 && hulk_type.back() == '*') {
            string base = hulk_type.substr(0, hulk_type.size() - 1);
            return "std::vector<" + toCppType(base) + ">";
        }

        // tipo usuario — puntero a clase
        return hulk_type + "*";
    }

    // Escapa nombres que colisionan con keywords de C++
    string toCppName(const string& name) {
        static const unordered_set<string> cpp_keywords = {
        // tipos primitivos que colisionan
        "double", "float", "int", "long", "short", "char",
        // otros que podrían usarse como nombres en HULK
        "class", "struct", "namespace", "template", "typename",
        "operator", "this", "void", "auto", "const", "static",
        "virtual", "override", "inline", "explicit", "friend", "negate"
    };
        if (cpp_keywords.count(name))
            return "_hulk_" + name;

        return name;
    }

    // Genera un nombre de variable temporal único
    int temp_counter = 0;
    string freshTemp() {
        return "_t" + to_string(temp_counter++);
    }

public:
    string generate(ProgramNode& program) {
        // header de runtime
        emit("#include \"hulk_runtime.h\"\n\n");

        // forward declarations
        for (auto& decl : program.declarations) {
            if (auto* tn = dynamic_cast<TypeNode*>(decl.get()))
                emitLine("struct " + tn->name + ";");
            else if (auto* pn = dynamic_cast<ProtocolNode*>(decl.get()))
                emitLine("struct " + pn->name + ";");
        }
        if (!program.declarations.empty()) emit("\n");

        // declaraciones
        program.accept(*this);

        return out.str();
    }

    void visit(ProgramNode& node) override {
        // ── forward declarations de funciones ──
        for (auto& decl : node.declarations) {
            if (auto* fn = dynamic_cast<FunctionNode*>(decl.get())) {
                bool needs_template = false;
                for (const auto& [pname, ptype] : fn->params)
                    if (ptype.empty()) { needs_template = true; break; }

                if (needs_template) {
                    emit("template<");
                    bool first = true;
                    for (const auto& [pname, ptype] : fn->params) {
                        if (ptype.empty()) {
                            if (!first) emit(", ");
                            emit("typename T_" + pname);
                            first = false;
                        }
                    }
                    emitLine(">");
                }

                string return_type = fn->return_type.empty() ? "auto" : toCppType(fn->return_type);
                emit(return_type + " " + toCppName(fn->name) + "(");
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i > 0) emit(", ");
                    const auto& [pname, ptype] = fn->params[i];
                    string cpp_type = ptype.empty() ? "T_" + pname : toCppType(ptype);
                    emit(cpp_type + " " + toCppName(pname));
                }
                emitLine(");");
            }
        }

        // ── definiciones de protocolos — antes que los tipos ──
        for (auto& decl : node.declarations)
            if (dynamic_cast<ProtocolNode*>(decl.get()))
                decl->accept(*this);

        // ── definiciones de tipos ──
        for (auto& decl : node.declarations)
            if (dynamic_cast<TypeNode*>(decl.get()))
                decl->accept(*this);

        // ── definiciones de funciones ──
        for (auto& decl : node.declarations)
            if (dynamic_cast<FunctionNode*>(decl.get()))
                decl->accept(*this);

        // ── main ──
        emitLine("int main() {");
        indent_level++;
        emit(indent());
        node.body->accept(*this);
        emit(";\n");
        emitLine("return 0;");
        indent_level--;
        emitLine("}");
    }

    void visit(NumberNode& node) override {
        if (node.value == (int)node.value)
            emit(to_string((int)node.value));
        else
            emit(to_string(node.value));
    }

    void visit(BoolNode& node) override {
        emit(node.value ? "true" : "false");
    }

    void visit(StringNode& node) override {
        emit("\"" + node.value + "\"");
    }

    void visit(IdentifierNode& node) override {
        if (node.name == "self") emit("this");
        else emit(toCppName(node.name));
    }

    void visit(UnaryOpNode& node) override {
        emit(node.op);
        emit("(");
        node.operand->accept(*this);
        emit(")");
    }

    void visit(BinaryOpNode& node) override {
        // concatenación — caso especial
        if (node.op == "@") {
            emit("hulk_concat(");
            node.left->accept(*this);
            emit(", ");
            node.right->accept(*this);
            emit(")");
            return;
        }
        if (node.op == "@@") {
            emit("hulk_concat_space(");
            node.left->accept(*this);
            emit(", ");
            node.right->accept(*this);
            emit(")");
            return;
        }

        // potencia y módulo — casos especiales
        if (node.op == "^") {
            emit("std::pow(");
            node.left->accept(*this);
            emit(", ");
            node.right->accept(*this);
            emit(")");
            return;
        }

        if (node.op == "%") {
            emit("std::fmod(");
            node.left->accept(*this);
            emit(", ");
            node.right->accept(*this);
            emit(")");
            return;
        }

        // resto de operadores — idénticos en C++
        emit("(");
        node.left->accept(*this);
        emit(" " + node.op + " ");
        node.right->accept(*this);
        emit(")");
    }

    void visit(IsNode& node) override {
        string expr_type = node.expr->inferred_type;

        // tipo primitivo — resultado estático
        if (expr_type == "Number" || expr_type == "Boolean" || expr_type == "String") {
            emit(expr_type == node.type_name ? "true" : "false");
            return;
        }

        // tipo usuario — dynamic_cast
        emit("(dynamic_cast<" + node.type_name + "*>(");
        node.expr->accept(*this);
        emit(") != nullptr)");
    }

    void visit(ReassignNode& node) override {
        node.target->accept(*this);
        emit(" = ");
        node.value->accept(*this);
    }

    void visit(BlockNode& node) override {
        // bloque en C++ — lambda inmediatamente invocada para que sea una expresión
        emit("[&]() {\n");
        indent_level++;

        for (size_t i = 0; i < node.expressions.size() - 1; ++i) {;
            emit(indent());
            node.expressions[i]->accept(*this);
            emit(";\n");
        }

        // última expresión es el valor de retorno
        emitLine();
        emit(indent() + "return ");
        node.expressions.back()->accept(*this);
        emit(";\n");

        indent_level--;
        emitLine("}()");
    }

    void visit(IfNode& node) override {
        // if en HULK es una expresión — lambda inmediatamente invocada
        emit("[&]() -> " + toCppType(node.inferred_type) + " {\n");
        indent_level++;

        for (auto& [cond, body] : node.branches) {
            emit(indent() + "if (");
            cond->accept(*this);
            emit(") return ");
            body->accept(*this);
            emit(";\n");
        }

        emit(indent() + "return ");
        node.else_body->accept(*this);
        emit(";\n");

        indent_level--;
        emitLine("}()");
    }

    void visit(WhileNode& node) override {
        // while también es expresión en HULK
        string result_var = freshTemp();
        emit("[&]() -> " + toCppType(node.inferred_type) + " {\n");
        indent_level++;

        // variable para capturar el último valor
        emitLine(toCppType(node.inferred_type) + " " + result_var + "{};");
        emit(indent() + "while (");
        node.condition->accept(*this);
        emit(") {\n");
        indent_level++;
        emit(indent() + result_var + " = ");
        node.body->accept(*this);
        emit(";\n");
        indent_level--;
        emitLine("}");
        emitLine("return " + result_var + ";");

        indent_level--;
        emitLine("}()");
    }

    void visit(ForNode& node) override {
        string result_var = freshTemp();
        string iter_var   = freshTemp();
        string iterable_type = node.iterable->inferred_type;

        // verificar si es un iterable usuario (no vector ni T*)
        bool is_user_iterable = !iterable_type.empty()
            && iterable_type.back() != '*'
            && (iterable_type.size() < 2 || iterable_type.substr(iterable_type.size()-2) != "[]");

        emit("[&]() -> " + toCppType(node.inferred_type) + " {\n");
        indent_level++;
        emitLine(toCppType(node.inferred_type) + " " + result_var + "{};");

        if (is_user_iterable) {
            // loop usando next()/current()
            emit(indent() + "auto " + iter_var + " = ");
            node.iterable->accept(*this);
            emit(";\n");
            emit(indent() + "while (" + iter_var + "->next()) {\n");
            indent_level++;
            emit(indent() + "auto " + node.variable + " = " + iter_var + "->current();\n");
            emit(indent() + result_var + " = ");
            node.body->accept(*this);
            emit(";\n");
            indent_level--;
            emitLine("}");
        } else {
            // range-for normal para vectores
            emit(indent() + "for (auto& " + node.variable + " : ");
            node.iterable->accept(*this);
            emit(") {\n");
            indent_level++;
            emit(indent() + result_var + " = ");
            node.body->accept(*this);
            emit(";\n");
            indent_level--;
            emitLine("}");
        }

        emitLine("return " + result_var + ";");
        indent_level--;
        emitLine("}()");
    }

    void visit(LetInNode& node) override {
        //Lambda para la creación de variables. Su retorno es el de su cuerpo
        emit("[&]() -> " + toCppType(node.inferred_type) + " {\n");
        indent_level++;

        for (auto& [name, type, value] : node.bindings) {
            emit(indent());
            // si tiene anotación de tipo la usamos, sino auto
            if (!type.empty())
                emit(toCppType(type) + " " + name + " = ");
            else
                emit("auto " + name + " = ");
            value->accept(*this);
            emit(";\n");
        }

        emit(indent() + "return ");
        node.body->accept(*this);
        emit(";\n");

        indent_level--;
        emitLine("}()");
    }

    void visit(FunctionNode& node) override {
        // lambda sin nombre — se emite inline
        if (node.name.empty()) {
            emit("[&](");
            for (size_t i = 0; i < node.params.size(); ++i) {
                if (i > 0) emit(", ");
                const auto& [pname, ptype] = node.params[i];
                string cpp_type = ptype.empty() ? "auto" : toCppType(ptype);
                emit(cpp_type + " " + toCppName(pname));
            }
            string return_type = node.return_type.empty() ? "auto" : toCppType(node.return_type);
            emit(") -> " + return_type + " {\n");
            indent_level++;
            emit(indent() + "return ");
            node.body->accept(*this);
            emit(";\n");
            indent_level--;
            emit(indent() + "}");
            return;
        }

        // ── función con nombre ──

        // verificar si algún parámetro no tiene tipo
        bool needs_template = false;
        for (const auto& [pname, ptype] : node.params)
            if (ptype.empty()) { needs_template = true; break; }

        // emitir template si hay parámetros sin tipo
        if (needs_template) {
            emit("template<");
            bool first = true;
            for (const auto& [pname, ptype] : node.params) {
                if (ptype.empty()) {
                    if (!first) emit(", ");
                    emit("typename T_" + pname);
                    first = false;
                }
            }
            emitLine(">");
        }

        // tipo de retorno — anotado si existe, sino auto
        string return_type = node.return_type.empty() ? "auto" : toCppType(node.return_type);
        emit(return_type + " " + toCppName(node.name) + "(");

        for (size_t i = 0; i < node.params.size(); ++i) {
            if (i > 0) emit(", ");
            const auto& [pname, ptype] = node.params[i];
            string cpp_type = ptype.empty() ? "T_" + pname : toCppType(ptype);
            emit(cpp_type + " " + toCppName(pname));
        }
        emit(") {\n");
        indent_level++;

        emit(indent() + "return ");
        node.body->accept(*this);
        emit(";\n");

        indent_level--;
        emitLine("}");
        emit("\n");
    }

    void visit(CallNode& node) override {
        // ── Caso 1: función builtin — mapear a hulk_*  ──
        if (auto* id = dynamic_cast<IdentifierNode*>(node.callee.get())) {
            string name = id->name;

            if (name == "print") {
                emit("hulk_print(");
                node.args[0]->accept(*this);
                emit(")");
                return;
            }
            if (name == "sqrt")  { emit("hulk_sqrt(");  node.args[0]->accept(*this); emit(")"); return; }
            if (name == "sin")   { emit("hulk_sin(");   node.args[0]->accept(*this); emit(")"); return; }
            if (name == "cos")   { emit("hulk_cos(");   node.args[0]->accept(*this); emit(")"); return; }
            if (name == "exp")   { emit("hulk_exp(");   node.args[0]->accept(*this); emit(")"); return; }
            if (name == "rand")  { emit("hulk_rand()"); return; }
            if (name == "log") {
                emit("hulk_log(");
                node.args[0]->accept(*this);
                emit(", ");
                node.args[1]->accept(*this);
                emit(")");
                return;
            }
            if (name == "range") {
                emit("hulk_range(");
                node.args[0]->accept(*this);
                emit(", ");
                node.args[1]->accept(*this);
                emit(")");
                return;
            }

            // Caso 2 y 3: constructor vs función
            bool is_constructor = (node.inferred_type == name);

            if (is_constructor)
                emit("new " + name + "(");
            else
                emit(toCppName(name) + "(");

            for (size_t i = 0; i < node.args.size(); ++i) {
                if (i > 0) emit(", ");
                node.args[i]->accept(*this);
            }
            emit(")");
            return;
        }

        // ── Caso 4: method call — obj.method(args) ──
        if (auto* ma = dynamic_cast<MemberAccessNode*>(node.callee.get())) {
            ma->object->accept(*this);

            // puntero a clase usuario → "->"
            // tipo primitivo → "."  (no debería ocurrir en HULK)
            string obj_type = ma->object->inferred_type;
            bool is_pointer = (obj_type != "Number" && obj_type != "Boolean" && obj_type != "String");
            emit(is_pointer ? "->" : ".");

            emit(toCppName(ma->member) + "(");
            for (size_t i = 0; i < node.args.size(); ++i) {
                if (i > 0) emit(", ");
                node.args[i]->accept(*this);
            }
            emit(")");
            return;
        }

        // ── Caso 5: functor — expr(args) ──
        emit("(");
        node.callee->accept(*this);
        emit(")(");
        for (size_t i = 0; i < node.args.size(); ++i) {
            if (i > 0) emit(", ");
            node.args[i]->accept(*this);
        }
        emit(")");
    }

    void visit(MemberAccessNode& node) override {
        string obj_type = node.object->inferred_type;
        bool is_pointer = (obj_type != "Number" && obj_type != "Boolean" && obj_type != "String");

        node.object->accept(*this);
        emit(is_pointer ? "->" : ".");
        emit(toCppName(node.member));
    }

    void visit(ProtocolNode& node) override {
        // los protocolos se convierten en clases abstractas C++
        emit("struct " + node.name);

        if (!node.parent.empty())
            emit(" : public " + node.parent);

        emit(" {\n");
        indent_level++;

        // destructor virtual — necesario para polimorfismo
        emitLine("virtual ~" + node.name + "() = default;");

        // métodos — todos virtuales puros
        for (const auto& method : node.methods) {
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                emit(indent() + "virtual " + toCppType(fn->return_type) + " " + fn->name + "(");
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i > 0) emit(", ");
                    const auto& [pname, ptype] = fn->params[i];
                    emit(toCppType(ptype) + " " + pname);
                }
                emit(") = 0;\n");
            }
        }

        indent_level--;
        emitLine("};");
        emit("\n");
    }

    void visit(TypeNode& node) override {
        // ── Declaración de clase ──
        emit("struct " + node.name);

        string base = node.parent.empty() ? "HulkObject" : node.parent;
        emit(" : public " + base);


        for (const auto& proto : node.satisfied_protocols) {
            if (proto == "Iterable") {
                // buscar tipo de retorno de current() en los métodos del tipo
                string current_type = "HulkObject*";
                for (const auto& method : node.methods) {
                    if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                        if (fn->name == "current" && !fn->return_type.empty())
                            current_type = toCppType(fn->return_type);
                    }
                }
                emit(", public Iterable<" + current_type + ">");
            } else {
                emit(", public " + proto);
            }
        }

        emit(" {\n");
        indent_level++;

        // ── Atributos ──
        for (const auto& [attr_name, attr_type, val] : node.attributes) {
            string cpp_type = attr_type.empty()
                ? toCppType(val->inferred_type)
                : toCppType(attr_type);
            emitLine(cpp_type + " " + toCppName(attr_name) + ";");
        }
        if (!node.attributes.empty()) emit("\n");

        // ── Constructor ──
        emit(indent() + node.name + "(");
        for (size_t i = 0; i < node.params.size(); ++i) {
            if (i > 0) emit(", ");
            const auto& [pname, ptype] = node.params[i];
            emit(toCppType(ptype.empty() ? "Object" : ptype) + " " + toCppName(pname));
        }
        emit(")");

        // inicialización del padre si hay herencia con args
        if (!node.parent.empty() && !node.parent_args.empty()) {
            emit(" : " + node.parent + "(");
            for (size_t i = 0; i < node.parent_args.size(); ++i) {
                if (i > 0) emit(", ");
                node.parent_args[i]->accept(*this);
            }
            emit(")");
        }

        emit(" {\n");
        indent_level++;

        // inicializar atributos en el cuerpo del constructor
        for (const auto& [attr_name, attr_type, val] : node.attributes) {
            emit(indent() + "this->" + attr_name + " = ");  // <- this->
            val->accept(*this);
            emit(";\n");
        }

        indent_level--;
        emitLine("}");
        emit("\n");

        // ── Métodos ──
        for (const auto& method : node.methods) {
            if (auto* fn = dynamic_cast<FunctionNode*>(method.get())) {
                string return_type;
                if (!fn->return_type.empty()) {
                    return_type = toCppType(fn->return_type);
                } else if (fn->body && !fn->body->inferred_type.empty()) {
                    return_type = toCppType(fn->body->inferred_type);
                } else {
                    return_type = "HulkObject*";  // fallback — no hay info de tipo
                }

                emit(indent() + "virtual " + return_type + " " + toCppName(fn->name) + "(");
                for (size_t i = 0; i < fn->params.size(); ++i) {
                    if (i > 0) emit(", ");
                    const auto& [pname, ptype] = fn->params[i];
                    emit(toCppType(ptype.empty() ? "Object" : ptype) + " " + toCppName(pname));
                }
                emit(") {\n");
                indent_level++;
                emit(indent() + "return ");
                fn->body->accept(*this);
                emit(";\n");
                indent_level--;
                emitLine("}");
                emit("\n");
            }
        }

        // toString para debugging
        emitLine("virtual std::string toString() const override {");
        indent_level++;
        emitLine("return \"<" + node.name + ">\";");
        indent_level--;
        emitLine("}");

        indent_level--;
        emitLine("};");
        emit("\n");
    }

    void visit(VectorNode& node) override {
        string elem_type = node.inferred_type.size() >= 2
            ? toCppType(node.inferred_type.substr(0, node.inferred_type.size() - 2))
            : "auto";

        emit("std::vector<" + elem_type + ">{");
        for (size_t i = 0; i < node.elements.size(); ++i) {
            if (i > 0) emit(", ");
            node.elements[i]->accept(*this);
        }
        emit("}");
    }

    void visit(VectorGeneratorNode& node) override {
        string elem_type = toCppType(node.inferred_type.substr(0, node.inferred_type.size() - 2));
        string vec_var   = freshTemp();

        emit("[&]() {\n");
        indent_level++;
        emitLine("std::vector<" + elem_type + "> " + vec_var + ";");
        emit(indent() + "for (auto& " + node.variable + " : ");
        node.iterable->accept(*this);
        emit(") {\n");
        indent_level++;
        emit(indent() + vec_var + ".push_back(");
        node.expr->accept(*this);
        emit(");\n");
        indent_level--;
        emitLine("}");
        emitLine("return " + vec_var + ";");
        indent_level--;
        emit(indent() + "}()");
    }

    void visit(IndexNode& node) override {
        node.object->accept(*this);
        emit("[");
        node.index->accept(*this);
        emit("]");
    }


};
