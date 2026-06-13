#ifndef EDUTRACE_OPTIMIZER_VISITOR_H
#define EDUTRACE_OPTIMIZER_VISITOR_H

#include "visitor.h"
#include "ast.h"
#include <iosfwd>

struct OptimizationStats {
    int constantFolds = 0;
    int algebraicSimplifications = 0;
    int deadStatementsRemoved = 0;
    int constantBranchesSimplified = 0;
    int constantLoopsRemoved = 0;

    int total() const {
        return constantFolds + algebraicSimplifications + deadStatementsRemoved
             + constantBranchesSimplified + constantLoopsRemoved;
    }
};

class OptimizerVisitor : public Visitor {
public:
    void optimize(Program& program);
    const OptimizationStats& getStats() const { return stats; }
    void printReport(std::ostream& out) const;

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
    OptimizationStats stats;

    void optimizeBlock(BlockStmt* block);
    void optimizeStmt(Stmt* stmt);
    void optimizeExpr(std::unique_ptr<Expr>& expr);
};

#endif
