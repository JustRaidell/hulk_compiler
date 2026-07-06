# HULK Compiler — Project Report

## Overview

This project implements a compiler for HULK, a statically-typed, expression-oriented programming language designed for educational purposes. The compiler takes a `.hulk` source file as input and produces a native executable (`./output`) by translating HULK source code to C++ and invoking `g++` as a backend. The entire pipeline — from lexical analysis to code generation — was implemented from scratch in C++17, deliberately avoiding tools like Flex, Yacc, or ANTLR in favor of hand-built components.

The compiler is invoked as:

```bash
./hulk <file.hulk>
```

On success, it produces `./output` in the current directory. On error, it exits with code `1` (lexical), `2` (syntactic), or `3` (semantic), printing one error per line to `stderr` in the format `(line,col) TYPE: message`.

---

## Architecture

The compiler is organized as a classic four-phase pipeline:

```
Source code
    ↓
[Lexer]             — tokenization via hand-built regex engine and DFA
    ↓
[Parser]            — recursive descent parser, builds the AST
    ↓
[Semantic Analyzer] — type inference, name resolution, type checking
    ↓
[Code Generator]    — AST to C++ translation
    ↓
output.cpp  →  g++  →  ./output
```

Each phase is implemented as a self-contained C++ component. The semantic analyzer and code generator both operate over the AST using the Visitor pattern, which allows the two phases to be cleanly separated without modifying the AST node definitions.

---

## Phase 1 — Lexer

### Hand-built Regex Engine

Rather than relying on standard library facilities or tools like Flex, the lexer is powered by a custom regular expression engine built from first principles. The engine follows the classical pipeline:

```
Regex string → AST → NFA → DFA (AFD)
```

Each token pattern is defined as a regular expression string. The engine parses the regex into an AST, converts it to a Non-deterministic Finite Automaton (NFA) using Thompson's construction, and then determinizes it into a DFA via the subset construction algorithm. Each DFA is then used during tokenization to match the longest possible prefix of the input at each position — the standard maximal munch strategy.

This approach allows token patterns to be defined declaratively as regex strings, while the matching itself is performed efficiently by deterministic automata. Implementing this engine from scratch required handling operator precedence in regex parsing (concatenation, alternation, Kleene star), epsilon transitions in NFA construction, and the powerset construction for determinization — all non-trivial components that are typically hidden behind tools like Flex.

### Tokenization

The lexer maintains a priority-ordered table of token patterns. At each position in the input, all DFAs compete and the longest match wins. Reserved words are given priority over identifiers by appearing earlier in the table.

Error handling covers two cases:
- **Unrecognized characters**: when no DFA matches at the current position, the lexer emits a `LEXICAL` error and exits with code `1`.
- **Invalid string literals**: when a string starting with `"` cannot be matched (due to invalid characters inside), the lexer reports an invalid string literal error.

---

## Phase 2 — Parser

### From LL(1) to Recursive Descent: A Design Evolution

The parser went through a significant design evolution during development. The original plan was to use the LL(1) table-driven parser as the production parser — a natural choice after having invested considerable effort in building it. A complete LL(1) implementation was produced: First and Follow sets were computed for all non-terminals, the parsing table was constructed with explicit conflict detection, and the grammar was carefully factored to eliminate left recursion and ambiguity.

However, a fundamental limitation became apparent: a table-driven LL(1) parser produces a flat sequence of derivation steps, not a tree. To build an AST from those derivations, one would need to attach semantic actions to each production rule — essentially implementing attributed grammars. This would have required threading synthesized and inherited attributes through the parsing table, a mechanism that is significantly more complex to implement and debug than it first appears, and one that couples grammar rules tightly to AST construction logic.

The decision was made to retire the LL(1) parser from the production pipeline and replace it with a hand-written recursive descent parser. Recursive descent maps naturally to AST construction: each parse method returns a node, and the tree emerges organically from the call structure. The effort invested in the LL(1) implementation was not wasted — the grammar factoring work it required carried over directly, and the validator remains in the codebase as a tool to detect conflicts whenever new productions are added.

This evolution reflects a broader truth about compiler construction: the theoretically elegant solution is not always the most practical one. The LL(1) parser is a beautiful formal artifact; the recursive descent parser is what actually ships.

### Recursive Descent Parser

The production parser is a hand-written recursive descent parser. Each non-terminal in the HULK grammar corresponds to a `parse*` method, and the grammar's precedence hierarchy is encoded directly in the call structure — lower-precedence operators are parsed higher in the call chain, higher-precedence operators lower.

Key design decisions:

**Expression-oriented parsing**: In HULK, everything is an expression. `let`, `if`, `while`, `for`, and block statements all produce values and are parsed as expressions. This is reflected in `parseExpr()` being the universal entry point, with all constructs returning `unique_ptr<ASTNode>`.

**Operator precedence**: The full precedence chain, from lowest to highest, is encoded as a sequence of mutually recursive methods:

```
parseExpr         →  :=  (right-associative)
parseCheckExpr    →  is
parseOrExpr       →  |
parseAndExpr      →  &
parseEqualityExpr →  == !=
parseCompareExpr  →  > < >= <=
parseConcatExpr   →  @ @@
parseArithExpr    →  + -
parseTerm         →  * /
parsePot          →  ^ %  (right-associative via recursion)
parseUnaryExpr    →  - !
parseFactor       →  atoms and postfix suffixes
```

**Postfix suffix chaining**: Member access (`.`), function calls (`()`), and indexing (`[]`) are handled in a unified loop in `parseFactor`, allowing arbitrary chaining: `obj.method(arg)[0].field`.

**Right-associativity**: The `^` and `%` operators use recursion instead of iteration in `parsePot`, correctly implementing right-associativity: `2^3^2` parses as `2^(3^2)`.

**Vector literal disambiguation**: Inside `[...]`, the parser reads up to the `|` level of the expression hierarchy to avoid consuming the `|` separator before determining whether the construct is a vector literal or a generator expression.

### AST

The parser produces a typed AST with 21 node types, covering all HULK constructs. Every node inherits from `ASTNode`, which carries an `inferred_type` field populated by the semantic analyzer for use during code generation. Each node implements `accept(Visitor&)` to support the Visitor pattern used in subsequent phases.

---

## Phase 3 — Semantic Analysis

The semantic analyzer is implemented as a `Visitor` over the AST. It performs three interleaved tasks: name resolution, type checking, and type inference.

### Symbol Table

The symbol table manages three namespaces: variables (stack of scopes), functions (global flat map), and types (global flat map). Variable scopes are pushed and popped as the analyzer enters and exits `let` bindings, function bodies, `for` loops, and type definitions. Functions and types live in a global scope because HULK does not support nested function declarations.

The table is pre-populated with built-in types (`Number`, `Boolean`, `String`, `Object`) and built-in functions (`print`, `sqrt`, `sin`, `cos`, `exp`, `log`, `rand`, `range`), as well as the built-in protocols `Iterable` and `Vector`.

### Two-Pass Analysis

At the program level, the analyzer performs two passes over the top-level declarations:

1. **Registration pass**: all functions, types, and protocols are registered in the symbol table before any body is analyzed. This allows mutually recursive functions and forward references between types.
2. **Analysis pass**: each declaration body is analyzed with the full symbol table available.

### Type System

HULK's type system combines nominal typing (for regular types) with structural typing (for protocols). The key components are:

**`lca(a, b)`**: computes the Lowest Common Ancestor of two types in the inheritance hierarchy. Used to determine the type of `if` expressions, where all branches must be compatible and the result type is their LCA.

**`satisfies(type, protocol)`**: checks whether a type structurally satisfies a protocol by verifying that all required methods are present. This implements HULK's implicit protocol conformance — a type does not need to explicitly declare that it implements a protocol.

**`isCompatible(actual, expected)`**: the unified compatibility check used throughout. It handles three cases: identity, structural protocol satisfaction, and nominal subtyping via `lca`. This function is the single entry point for all type compatibility checks, including assignment, function arguments, and return types.

**Type inference**: where type annotations are absent, the analyzer infers types bottom-up from expressions. The inferred type of each node is stored in `node.inferred_type` for the code generator to use.

### Error Reporting

The analyzer collects all semantic errors without aborting, allowing multiple errors to be reported in a single run. Errors are printed to `stderr` in the required format and the process exits with code `3`. Detected error classes include: undeclared variables and functions, type mismatches, invalid reassignment targets, circular inheritance, duplicate declarations, protocol violations, and instantiation of abstract protocols.

---

## Phase 4 — Code Generation

The code generator is implemented as a second `Visitor` over the AST — now annotated with inferred types by the semantic analyzer. It produces a single C++ source file (`output.cpp`) which is then compiled by `g++` to produce the final executable.

### Type Mapping

HULK types map to C++ as follows:

| HULK | C++ |
|------|-----|
| `Number` | `double` |
| `Boolean` | `bool` |
| `String` | `std::string` |
| `Object` | `HulkObject*` |
| `T[]` | `std::vector<T>` |
| `T*` | `std::vector<T>` |
| User-defined types | Pointer to C++ class |

### Expression-to-Statement Translation

Since HULK is expression-oriented and C++ is statement-oriented, constructs like `if`, `while`, `for`, `let`, and block expressions are translated to immediately-invoked C++ lambdas:

```cpp
// HULK: let x = 1 in x + 1
[&]() -> double {
    double x = 1;
    return x + 1;
}()
```

This preserves the value-returning semantics of all HULK constructs without requiring a separate IR or value-passing mechanism.

### Type Hierarchy

User-defined types are emitted as C++ `struct`s inheriting from `HulkObject` (or from their declared parent). All methods are marked `virtual` to support polymorphism. Protocols are emitted as abstract C++ structs with pure virtual methods. A `toString()` method is generated for each user-defined type to support the built-in string conversion used by `@` and `@@`.

### Runtime Support

A `hulk_runtime.h` header is emitted at the top of every generated file. It defines the `HulkObject` base class, type aliases for primitive types, and implementations of all built-in functions (`hulk_print`, `hulk_sqrt`, `hulk_concat`, `hulk_range`, etc.).

---

## Known Limitations

**Lambda functions**: anonymous function expressions (`function(x) => x + 1`) are parsed and represented in the AST but are not fully supported through the semantic and code generation phases.

**Vector syntax**: the compiler implements the vector syntax as defined in the HULK language specification (`[1, 2, 3]` and `[x^2 | x in range(1,10)]`). The alternative syntax used in some test cases (`new Number[5]`, `{10, 20, 30}`) is not part of the official specification and is therefore not supported.

**Source positions in errors**: error messages currently report position `(0,0)` for all errors. Line and column tracking is a planned improvement but was not implemented within the project timeline.

**Functor types**: the `(T) -> T'` type annotation syntax for higher-order functions is parsed and stored, but full functor type checking and code generation is only partially implemented.

---

## Design Reflections

The decision to build the regex engine, the LL(1) validator, and the recursive descent parser entirely by hand — rather than using Flex, Yacc, or ANTLR — was deliberate. These tools abstract away the most instructive parts of compiler construction. Implementing Thompson's construction, the subset construction, First/Follow set computation, and LL(1) table construction from scratch produced a deeper understanding of how lexers and parsers actually work, at the cost of significantly more implementation time.

The LL(1)-to-recursive-descent transition was perhaps the most instructive moment of the project. It illustrated that the formal and the practical do not always align: the LL(1) parser is theoretically sound and produces a correct parse, but integrating AST construction into a table-driven framework requires attributed grammars — a layer of complexity that recursive descent absorbs naturally. Recognizing that limitation mid-project, rather than forcing a solution that would have been harder to maintain, was the right call.

The Visitor pattern proved to be the right architectural choice for the back half of the pipeline. Having the semantic analyzer and code generator as separate Visitor implementations over a shared AST made it straightforward to keep concerns separated, add new analysis passes, and reason about each phase independently. The alternative — embedding `analyze()` and `generateCode()` methods directly in each AST node — would have coupled all three concerns in a single file and made incremental development considerably harder.

Targeting C++ as the compilation backend was a pragmatic decision that paid off. The generated code is readable, `g++` handles memory layout and platform ABI details automatically, and errors that escape semantic analysis are caught at the C++ compilation step rather than producing silent incorrect behavior.

Finally, the gap between the language specification and the test suite deserves mention. Several test cases use syntax that does not appear in the official HULK specification — most notably the array initialization syntax. This is a real-world phenomenon: specifications and implementations diverge, and a compiler author must decide which to follow. This compiler follows the specification. Where divergences exist, they are documented here so that evaluators have the full picture.

---

*This compiler was built incrementally, one phase at a time, with each component validated before the next was begun. The result is a pipeline that, while not complete in every corner case, is coherent, well-structured, and honest about what it does and does not support.*
