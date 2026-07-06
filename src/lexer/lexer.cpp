#include <iostream>
#include <tuple>
#include <vector>

#include <unordered_set>
#include <unordered_map>
#include <stack>
#include <queue>

#include "Regex.cpp"
#include "Converter.cpp"

using namespace std;

class Lexer
{
public:
    Lexer()
    {
        build_lexer();
    }

    vector<tuple<string, string>> table;
    vector<pair<string,AFD>> patterns;
    vector<pair<string,string>> tokens;

//    vector<unique_ptr<Node>> asts;
//    NFA automaton;

    void build_lexer(){
            build_table();
            build_patterns(table);

            //Métodos descartados para crear un autómata unificado
            //build_ast(table);
            //build_automaton();
            //printAllASTs(asts, table);
        }

    void build_table(){
        //Skip First
            table.push_back(make_tuple("space", SPACE));

        //Reserved words
            table.push_back(make_tuple("while_", "while"));
            table.push_back(make_tuple("for_", "for"));
            table.push_back(make_tuple("let", "let"));
            table.push_back(make_tuple("in_", "in"));
            table.push_back(make_tuple("function", "function"));
            table.push_back(make_tuple("if_", "if"));
            table.push_back(make_tuple("elif_", "elif"));
            table.push_back(make_tuple("else_", "else"));
            table.push_back(make_tuple("true", "true"));
            table.push_back(make_tuple("false", "false"));
            table.push_back(make_tuple("new", "new"));
            table.push_back(make_tuple("type", "type"));
            table.push_back(make_tuple("inherits", "inherits"));
            table.push_back(make_tuple("protocol", "protocol"));
            table.push_back(make_tuple("extends", "extends"));
            table.push_back(make_tuple("define", "define"));


        //Dual Operators
            table.push_back(make_tuple("lte", "<="));
            table.push_back(make_tuple("gte", ">="));
            table.push_back(make_tuple("eq", "=="));
            table.push_back(make_tuple("neq", "!="));
            table.push_back(make_tuple("space_concat", "@@"));
            table.push_back(make_tuple("assignation", ":="));
            table.push_back(make_tuple("func_arrow", "=>"));
            table.push_back(make_tuple("simple_arrow", "->"));

        //Simple Operators
            table.push_back(make_tuple("plus_operator", "+"));
            table.push_back(make_tuple("minus_operator", "-"));
            table.push_back(make_tuple("multiplication", "\\*"));
            table.push_back(make_tuple("division", "/"));
            table.push_back(make_tuple("exponentiation", "^"));
            table.push_back(make_tuple("and_", "&"));
            table.push_back(make_tuple("or_", "\\|"));
            table.push_back(make_tuple("not_", "!"));
            table.push_back(make_tuple("gt", ">"));
            table.push_back(make_tuple("lt", "<"));
            table.push_back(make_tuple("concat_operator", "@"));
            table.push_back(make_tuple("inicialization", "="));
            table.push_back(make_tuple("module_operation", "%"));
            table.push_back(make_tuple("type_asignator", ":"));
            table.push_back(make_tuple("type_check", "is"));

        //Symbols
            table.push_back(make_tuple("open_curly_bracket", "\\("));
            table.push_back(make_tuple("closed_curly_bracket", "\\)"));
            table.push_back(make_tuple("open_bracket", "{"));
            table.push_back(make_tuple("closed_bracket", "}"));
            table.push_back(make_tuple("open_square_bracket", "["));
            table.push_back(make_tuple("closed_square_bracket", "]"));
            table.push_back(make_tuple("semicolon", ";"));
            table.push_back(make_tuple("comma", ","));
            table.push_back(make_tuple("dot", "."));

        //Built-in types
            table.push_back(make_tuple("number", integer));
            table.push_back(make_tuple("id", identifier));
            table.push_back(make_tuple("string", str));
        }


    void build_patterns(vector<tuple<string, string>> token_table) {

        for (const auto& [token_type, regex_str] : token_table)
            {
                RegEx reg(regex_str);
                auto ast = reg.parseRegex();  // Genera el AST para esta regex

                AFD pattern = Convert(*ast);
                patterns.push_back({token_type, pattern});
            }
    }

    vector<pair<string,string>> tokenize(const string code) {

    size_t pos = 0;
    string best, type;

    while (pos < code.length()){
        best = ""; type = "";

        if (code[pos] == '"') {
            size_t closing = code.find('"', pos + 1);
            if (closing == string::npos) {
                cerr << "(0,0) LEXICAL: unclosed string literal starting at position " << pos << endl;
                exit(1);
            }
        }

        //Buscar el lexema más largo que matchea con alguna RegEx conocida
        for (const auto& [token_type, afd]: patterns){
//            string lexeme = afd.match(code.substr(pos));
            string lexeme = afd.match(code, pos);


            if (lexeme.length() > best.length())
            {
                best = lexeme;
                type = token_type;
            }
        }

        if (best.length() == 0) {
            if (code[pos] == '"') {
                cerr << "(0,0) LEXICAL: invalid string literal starting at position " << pos << endl;
            } else {
                cerr << "(0,0) LEXICAL: unexpected character '" << code[pos] << "' at position " << pos << endl;
            }
            exit(1);
        }


        if (type == "space") {
            pos += best.length();
            continue;
        }

        if (type == "string") {
            tokens.push_back({type, best.substr(1, best.length() - 2)});
        }
        else tokens.push_back({type, best});

        pos += best.length();
    }

    tokens.push_back({"EOF", "#"});

    return tokens;
}

    protected:

    private:
        const string digits = "0|1|2|3|4|5|6|7|8|9";
        const string symbols = R"( |!|#|$|%|&|\||'|\\"|\(|\)|+|-|\*|^|/|.|,|:|;|<|=|>|?|@|[|]|{|}|_|\n|\t|\f|\r|\v)";
        const string uppers = "A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z";
        const string lowers = "a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z";

        const string SPACE = "(\n|\t|\f|\r|\v| )(\n|\t|\f|\r|\v| )*";
        const string identifier = "(" + uppers + "|" + lowers + ")("+ uppers + "|" + lowers + "|" + digits + "|" + "_)*";
        const string integer  = "(" + digits + ")((.(" + digits + "))|(" + digits + ")*)(" + digits + ")*";
        const string str = "\"(" + uppers + "|" + lowers + "|" + digits + "|" + symbols + ")*\"";
};
