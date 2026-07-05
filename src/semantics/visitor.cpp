using namespace std;
//#include "../parser/ast_parser.cpp"
#include "visitor.h"

// ─── Visitor base ─────────────────────────────────────────────────────────────

struct Visitor {
    virtual void visit(ProgramNode&)         {}
    virtual void visit(NumberNode&)          {}
    virtual void visit(BoolNode&)            {}
    virtual void visit(StringNode&)          {}
    virtual void visit(IdentifierNode&)      {}
    virtual void visit(IsNode&)              {}
    virtual void visit(BinaryOpNode&)        {}
    virtual void visit(UnaryOpNode&)         {}
    virtual void visit(ReassignNode&)        {}
    virtual void visit(CallNode&)            {}
    virtual void visit(MemberAccessNode&)    {}
    virtual void visit(IndexNode&)           {}
    virtual void visit(VectorNode&)          {}
    virtual void visit(VectorGeneratorNode&) {}
    virtual void visit(LetInNode&)           {}
    virtual void visit(IfNode&)              {}
    virtual void visit(WhileNode&)           {}
    virtual void visit(ForNode&)             {}
    virtual void visit(BlockNode&)           {}
    virtual void visit(FunctionNode&)        {}
    virtual void visit(TypeNode&)            {}
    virtual void visit(ProtocolNode&)        {}

    virtual ~Visitor() = default;
};
