#ifndef EDUTRACE_TYPECHECK_VISITOR_H
#define EDUTRACE_TYPECHECK_VISITOR_H

#include <unordered_map>
#include <unordered_set>
#include "visitor.h"
#include "ast.h"
#include "symbol_table.h"

class TypeCheckVisitor : public Visitor {
public:
    void check(Program& program);

    void visit(Program* node) override;
    void visit(StructDecl* node) override;
    void visit(FunctionDecl* node) override;
    void visit(BlockStmt* node) override;
    void visit(VarDeclStmt* node) override;
    void visit(AssignStmt* node) override;
    void visit(ArrayAssignStmt* node) override;
    void visit(PointerAssignStmt* node) override;
    void visit(FieldAssignStmt* node) override;
    void visit(DeleteStmt* node) override;
    void visit(PrintStmt* node) override;
    void visit(ReturnStmt* node) override;
    void visit(IfStmt* node) override;
    void visit(WhileStmt* node) override;
    void visit(TraceStmt* node) override;
    void visit(ExprStmt* node) override;

    void visit(BinaryExpr* node) override;
    void visit(UnaryExpr* node) override;
    void visit(IntExpr* node) override;
    void visit(BoolExpr* node) override;
    void visit(CharExpr* node) override;
    void visit(StringExpr* node) override;
    void visit(IdExpr* node) override;
    void visit(ArrayAccessExpr* node) override;
    void visit(FieldAccessExpr* node) override;
    void visit(NewExpr* node) override;
    void visit(LambdaExpr* node) override;
    void visit(CallExpr* node) override;

private:
    Program* program = nullptr;
    SymbolTable symbols;
    std::unordered_map<std::string, FunctionDecl*> functions;
    std::unordered_map<std::string, StructDecl*> structs;
    Type currentFunctionReturn = Type::Void();
    Type lastType = Type::Unknown();

    Type infer(Expr* expr);
    void validateKnownType(const Type& type, const SourceLocation& loc);
    Type fieldType(const std::string& objectName, const std::string& fieldName, const SourceLocation& loc);
    void requireCondition(const Type& type, const SourceLocation& loc, const std::string& constructName);
    Type arrayElementType(const std::string& name, const std::vector<Expr*>& indices, const SourceLocation& loc);
    Type specializeType(const Type& type, const std::string& genericName, const Type& concrete) const;
    bool containsGeneric(const Type& type) const;
};

#endif
