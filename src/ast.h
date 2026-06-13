#ifndef EDUTRACE_AST_H
#define EDUTRACE_AST_H

#include <memory>
#include <string>
#include <vector>
#include "types.h"
#include "errors.h"

class Visitor;

class Node {
public:
    SourceLocation loc;
    explicit Node(SourceLocation location = {}) : loc(location) {}
    virtual ~Node() = default;
    virtual void accept(Visitor& visitor) = 0;
};

class Expr : public Node {
public:
    using Node::Node;
};

class Stmt : public Node {
public:
    using Node::Node;
};

struct Param {
    Type type;
    std::string name;
    SourceLocation loc;
};

enum class TraceKind {
    Variables,
    Loop,
    Branch,
    Stack,
    Recursion,
    Array,
    Memory
};

std::string traceKindName(TraceKind kind);

class FunctionDecl;
class StructDecl;

class Program : public Node {
public:
    std::vector<std::unique_ptr<StructDecl>> structs;
    std::vector<std::unique_ptr<FunctionDecl>> functions;
    using Node::Node;
    FunctionDecl* findFunction(const std::string& name) const;
    void accept(Visitor& visitor) override;
};

class StructDecl : public Node {
public:
    std::string name;
    std::vector<Param> fields;

    StructDecl(std::string n, std::vector<Param> fs, SourceLocation loc = {});
    const Param* findField(const std::string& fieldName) const;
    void accept(Visitor& visitor) override;
};

class FunctionDecl : public Node {
public:
    Type returnType;
    std::string name;
    std::string templateParam;
    std::vector<Param> params;
    std::unique_ptr<class BlockStmt> body;

    FunctionDecl(Type ret, std::string n, std::vector<Param> ps,
                 std::unique_ptr<BlockStmt> b, SourceLocation loc = {}, std::string templateParam = "");
    bool isTemplate() const;
    void accept(Visitor& visitor) override;
};

class BlockStmt : public Stmt {
public:
    std::vector<std::unique_ptr<Stmt>> statements;
    using Stmt::Stmt;
    void accept(Visitor& visitor) override;
};

class VarDeclStmt : public Stmt {
public:
    Type type;
    std::string name;
    std::unique_ptr<Expr> initializer;
    bool inferred = false;

    VarDeclStmt(Type t, std::string n, std::unique_ptr<Expr> init, bool inf, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class AssignStmt : public Stmt {
public:
    std::string name;
    std::unique_ptr<Expr> value;

    AssignStmt(std::string n, std::unique_ptr<Expr> v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class ArrayAssignStmt : public Stmt {
public:
    std::string name;
    std::vector<std::unique_ptr<Expr>> indices;
    std::unique_ptr<Expr> value;

    ArrayAssignStmt(std::string n, std::vector<std::unique_ptr<Expr>> idxs, std::unique_ptr<Expr> v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};


class PointerAssignStmt : public Stmt {
public:
    std::string pointerName;
    std::unique_ptr<Expr> value;

    PointerAssignStmt(std::string ptr, std::unique_ptr<Expr> v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class DeleteStmt : public Stmt {
public:
    std::string pointerName;

    DeleteStmt(std::string ptr, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class FieldAssignStmt : public Stmt {
public:
    std::string objectName;
    std::string fieldName;
    std::unique_ptr<Expr> value;

    FieldAssignStmt(std::string obj, std::string field, std::unique_ptr<Expr> v, SourceLocation loc = {});
    std::string fullName() const;
    void accept(Visitor& visitor) override;
};

class PrintStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    explicit PrintStmt(std::unique_ptr<Expr> expr, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class ReturnStmt : public Stmt {
public:
    std::unique_ptr<Expr> value;
    explicit ReturnStmt(std::unique_ptr<Expr> v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class IfStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> thenBranch;
    std::unique_ptr<BlockStmt> elseBranch;

    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> thenB,
           std::unique_ptr<BlockStmt> elseB, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class WhileStmt : public Stmt {
public:
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> body;

    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> b, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class TraceStmt : public Stmt {
public:
    TraceKind kind;
    std::vector<std::string> names;

    TraceStmt(TraceKind k, std::vector<std::string> ns, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class ExprStmt : public Stmt {
public:
    std::unique_ptr<Expr> expression;
    explicit ExprStmt(std::unique_ptr<Expr> expr, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class BinaryExpr : public Expr {
public:
    std::unique_ptr<Expr> left;
    std::string op;
    std::unique_ptr<Expr> right;

    BinaryExpr(std::unique_ptr<Expr> l, std::string o, std::unique_ptr<Expr> r, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class UnaryExpr : public Expr {
public:
    std::string op;
    std::unique_ptr<Expr> right;

    UnaryExpr(std::string o, std::unique_ptr<Expr> r, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class IntExpr : public Expr {
public:
    long value;
    explicit IntExpr(long v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class BoolExpr : public Expr {
public:
    bool value;
    explicit BoolExpr(bool v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class CharExpr : public Expr {
public:
    char value;
    explicit CharExpr(char v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class StringExpr : public Expr {
public:
    std::string value;
    explicit StringExpr(std::string v, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class IdExpr : public Expr {
public:
    std::string name;
    explicit IdExpr(std::string n, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class ArrayAccessExpr : public Expr {
public:
    std::string name;
    std::vector<std::unique_ptr<Expr>> indices;

    ArrayAccessExpr(std::string n, std::vector<std::unique_ptr<Expr>> idxs, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};


class FieldAccessExpr : public Expr {
public:
    std::string objectName;
    std::string fieldName;

    FieldAccessExpr(std::string obj, std::string field, SourceLocation loc = {});
    std::string fullName() const;
    void accept(Visitor& visitor) override;
};

class NewExpr : public Expr {
public:
    Type allocatedType;

    explicit NewExpr(Type t, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class LambdaExpr : public Expr {
public:
    Type returnType;
    std::vector<Param> params;
    std::unique_ptr<BlockStmt> body;

    LambdaExpr(Type ret, std::vector<Param> ps, std::unique_ptr<BlockStmt> b, SourceLocation loc = {});
    void accept(Visitor& visitor) override;
};

class CallExpr : public Expr {
public:
    std::string callee;
    std::vector<Type> typeArguments;
    std::vector<std::unique_ptr<Expr>> arguments;

    CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> args, SourceLocation loc = {}, std::vector<Type> typeArgs = {});
    void accept(Visitor& visitor) override;
};

#endif
