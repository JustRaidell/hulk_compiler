#include <iostream>
#include <fstream>


#include "src/lexer/lexer.cpp"
#include "src/parser/ast_parser.cpp"
#include "src/semantics/semantic.cpp"
#include "src/codegen/generator.cpp"

#include "src/parser/parsers.cpp"

#include "src/lexer/Regex.cpp"
#include "src/lexer/Converter.cpp"

using namespace std;

vector<Symbol> tokensToSymbols(vector<pair<string,string>> tokens){
    vector<Symbol> symbols;

    for (const auto& [type, lexeme]: tokens){
        if (lexeme == "range"){ symbols.push_back(Symbol(lexeme, true)); continue; }

        if (lexeme == "Number" || lexeme == "Boolean" || lexeme == "String" || lexeme == "Object" || lexeme == "Any")
            { symbols.push_back(Symbol(lexeme, true)); continue; }

        if (type != "id" && type != "number" && type != "string") symbols.push_back(Symbol(lexeme, true));
        else symbols.push_back(Symbol(type, true));
    }

    return symbols;
}

string LoadCode (int argc, char* argv[]) {
    string code;

    // Si no hay argumentos, usar código de prueba
    if (argc == 1) {
        code = R"(

print("Hello world!");

    )";

        cout << "=== Usando codigo de prueba ===" << endl;
    }
    // Si hay un argumento, leer el archivo
    else if (argc == 2) {
        ifstream archivo(argv[1]);

        if (!archivo.is_open()) {
            cerr << "Error: No se pudo abrir el archivo '" << argv[1] << "'" << endl;
            return "";
        }

        string linea;
        while (getline(archivo, linea)) {
            code += linea + "\n";
        }

        archivo.close();
        cout << "=== Compilando archivo: " << argv[1] << " ===" << endl;
    }
    // Demasiados argumentos
    else {
        cerr << "Uso: " << argv[0] << " [archivo.hulk]" << endl;
        cerr << "Si no se especifica archivo, se usa código de prueba." << endl;
        return "";
    }

    return code;
}

int main(int argc, char* argv[])
{
    string code = LoadCode(argc, argv);
    srand(time(nullptr));


    Lexer lexer;
    auto tokens = lexer.tokenize(code);

    RecursiveDescentParser parser(tokens);
    auto ast = parser.parse();

    SemanticAnalyzer analyzer;
    if (!analyzer.analyze(static_cast<ProgramNode&>(*ast))) return 3;

    CodeGenerator generator;
    string cpp_code = generator.generate(static_cast<ProgramNode&>(*ast));



    // escribir output.cpp
    ofstream output_file("output.cpp");
    if (!output_file.is_open()) {
        cerr << "Error: No se pudo crear output.cpp\n";
        return 4;
    }
    output_file << cpp_code;
    output_file.close();

    int result = system("g++ -std=c++17 -static -o output output.cpp");
    if (result != 0) {
        cerr << "Error: g++ ha fallado al compilar output.cpp\n";
        return 4;
    }

    //cout << "Compilado exitosamente hacia ./output\n";
}
