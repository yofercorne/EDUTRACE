#include "ast.h"
#include "visitor.h"

#include <utility>

std::string traceKindName(TraceKind kind) {
    switch (kind) {
        case TraceKind::Variables: return "variables";
        case TraceKind::Loop: return "loop";
        case TraceKind::Branch: return "branch";
        case TraceKind::Stack: return "stack";
        case TraceKind::Recursion: return "recursion";
        case TraceKind::Array: return "array";
        case TraceKind::Memory: return "memory";
    }
    return "unknown";
}

FunctionDecl* Program::findFunction(const std::string& name) const {
    for (const auto& fn : functions) {
        if (fn->name == name) return fn.get();
    }
    return nullptr;
}

void Program::accept(Visitor& visitor) { visitor.visit(this); }

StructDecl::StructDecl(std::string n, std::vector<Param> fs, SourceLocation location)
    : Node(location), name(std::move(n)), fields(std::move(fs)) {}

const Param* StructDecl::findField(const std::string& fieldName) const {
    for (const auto& field : fields) {
        if (field.name == fieldName) return &field;
    }
    return nullptr;
}

void StructDecl::accept(Visitor& visitor) { visitor.visit(this); }

FunctionDecl::FunctionDecl(Type ret, std::string n, std::vector<Param> ps,
                           std::unique_ptr<BlockStmt> b, SourceLocation location, std::string tmpl)
    : Node(location), returnType(std::move(ret)), name(std::move(n)), templateParam(std::move(tmpl)), params(std::move(ps)), body(std::move(b)) {}

bool FunctionDecl::isTemplate() const { return !templateParam.empty(); }

void FunctionDecl::accept(Visitor& visitor) { visitor.visit(this); }
void BlockStmt::accept(Visitor& visitor) { visitor.visit(this); }

VarDeclStmt::VarDeclStmt(Type t, std::string n, std::unique_ptr<Expr> init, bool inf, SourceLocation location)
    : Stmt(location), type(std::move(t)), name(std::move(n)), initializer(std::move(init)), inferred(inf) {}

void VarDeclStmt::accept(Visitor& visitor) { visitor.visit(this); }

AssignStmt::AssignStmt(std::string n, std::unique_ptr<Expr> v, SourceLocation location)
    : Stmt(location), name(std::move(n)), value(std::move(v)) {}

void AssignStmt::accept(Visitor& visitor) { visitor.visit(this); }

ArrayAssignStmt::ArrayAssignStmt(std::string n, std::vector<std::unique_ptr<Expr>> idxs, std::unique_ptr<Expr> v, SourceLocation location)
    : Stmt(location), name(std::move(n)), indices(std::move(idxs)), value(std::move(v)) {}

void ArrayAssignStmt::accept(Visitor& visitor) { visitor.visit(this); }

PointerAssignStmt::PointerAssignStmt(std::string ptr, std::unique_ptr<Expr> v, SourceLocation location)
    : Stmt(location), pointerName(std::move(ptr)), value(std::move(v)) {}

void PointerAssignStmt::accept(Visitor& visitor) { visitor.visit(this); }

DeleteStmt::DeleteStmt(std::string ptr, SourceLocation location)
    : Stmt(location), pointerName(std::move(ptr)) {}

void DeleteStmt::accept(Visitor& visitor) { visitor.visit(this); }

FieldAssignStmt::FieldAssignStmt(std::string obj, std::string field, std::unique_ptr<Expr> v, SourceLocation location)
    : Stmt(location), objectName(std::move(obj)), fieldName(std::move(field)), value(std::move(v)) {}

std::string FieldAssignStmt::fullName() const { return objectName + "." + fieldName; }

void FieldAssignStmt::accept(Visitor& visitor) { visitor.visit(this); }

PrintStmt::PrintStmt(std::unique_ptr<Expr> expr, SourceLocation location)
    : Stmt(location), expression(std::move(expr)) {}

void PrintStmt::accept(Visitor& visitor) { visitor.visit(this); }

ReturnStmt::ReturnStmt(std::unique_ptr<Expr> v, SourceLocation location)
    : Stmt(location), value(std::move(v)) {}

void ReturnStmt::accept(Visitor& visitor) { visitor.visit(this); }

IfStmt::IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> thenB,
               std::unique_ptr<BlockStmt> elseB, SourceLocation location)
    : Stmt(location), condition(std::move(cond)), thenBranch(std::move(thenB)), elseBranch(std::move(elseB)) {}

void IfStmt::accept(Visitor& visitor) { visitor.visit(this); }

WhileStmt::WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<BlockStmt> b, SourceLocation location)
    : Stmt(location), condition(std::move(cond)), body(std::move(b)) {}

void WhileStmt::accept(Visitor& visitor) { visitor.visit(this); }

TraceStmt::TraceStmt(TraceKind k, std::vector<std::string> ns, SourceLocation location)
    : Stmt(location), kind(k), names(std::move(ns)) {}

void TraceStmt::accept(Visitor& visitor) { visitor.visit(this); }

ExprStmt::ExprStmt(std::unique_ptr<Expr> expr, SourceLocation location)
    : Stmt(location), expression(std::move(expr)) {}

void ExprStmt::accept(Visitor& visitor) { visitor.visit(this); }

BinaryExpr::BinaryExpr(std::unique_ptr<Expr> l, std::string o, std::unique_ptr<Expr> r, SourceLocation location)
    : Expr(location), left(std::move(l)), op(std::move(o)), right(std::move(r)) {}

void BinaryExpr::accept(Visitor& visitor) { visitor.visit(this); }

UnaryExpr::UnaryExpr(std::string o, std::unique_ptr<Expr> r, SourceLocation location)
    : Expr(location), op(std::move(o)), right(std::move(r)) {}

void UnaryExpr::accept(Visitor& visitor) { visitor.visit(this); }

IntExpr::IntExpr(long v, SourceLocation location) : Expr(location), value(v) {}
void IntExpr::accept(Visitor& visitor) { visitor.visit(this); }

BoolExpr::BoolExpr(bool v, SourceLocation location) : Expr(location), value(v) {}
void BoolExpr::accept(Visitor& visitor) { visitor.visit(this); }

CharExpr::CharExpr(char v, SourceLocation location) : Expr(location), value(v) {}
void CharExpr::accept(Visitor& visitor) { visitor.visit(this); }

StringExpr::StringExpr(std::string v, SourceLocation location) : Expr(location), value(std::move(v)) {}
void StringExpr::accept(Visitor& visitor) { visitor.visit(this); }

IdExpr::IdExpr(std::string n, SourceLocation location) : Expr(location), name(std::move(n)) {}
void IdExpr::accept(Visitor& visitor) { visitor.visit(this); }

ArrayAccessExpr::ArrayAccessExpr(std::string n, std::vector<std::unique_ptr<Expr>> idxs, SourceLocation location)
    : Expr(location), name(std::move(n)), indices(std::move(idxs)) {}

void ArrayAccessExpr::accept(Visitor& visitor) { visitor.visit(this); }

FieldAccessExpr::FieldAccessExpr(std::string obj, std::string field, SourceLocation location)
    : Expr(location), objectName(std::move(obj)), fieldName(std::move(field)) {}

std::string FieldAccessExpr::fullName() const { return objectName + "." + fieldName; }

void FieldAccessExpr::accept(Visitor& visitor) { visitor.visit(this); }

NewExpr::NewExpr(Type t, SourceLocation location)
    : Expr(location), allocatedType(std::move(t)) {}

void NewExpr::accept(Visitor& visitor) { visitor.visit(this); }

LambdaExpr::LambdaExpr(Type ret, std::vector<Param> ps, std::unique_ptr<BlockStmt> b, SourceLocation location)
    : Expr(location), returnType(std::move(ret)), params(std::move(ps)), body(std::move(b)) {}

void LambdaExpr::accept(Visitor& visitor) { visitor.visit(this); }

CallExpr::CallExpr(std::string c, std::vector<std::unique_ptr<Expr>> args, SourceLocation location, std::vector<Type> typeArgs)
    : Expr(location), callee(std::move(c)), typeArguments(std::move(typeArgs)), arguments(std::move(args)) {}

void CallExpr::accept(Visitor& visitor) { visitor.visit(this); }
