#pragma once

// ─── Forward declarations ─────────────────────────────────────────────────────
// Le decimos al compilador que estos tipos existen
// sin necesidad de conocer su definición todavía

struct ProgramNode;
struct NumberNode;
struct BoolNode;
struct StringNode;
struct IdentifierNode;
struct IsNode;
struct BinaryOpNode;
struct UnaryOpNode;
struct ReassignNode;
struct CallNode;
struct MemberAccessNode;
struct IndexNode;
struct VectorNode;
struct VectorGeneratorNode;
struct LetInNode;
struct IfNode;
struct WhileNode;
struct ForNode;
struct BlockNode;
struct FunctionNode;
struct TypeNode;
struct ProtocolNode;

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
