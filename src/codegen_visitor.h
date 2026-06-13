#ifndef EDUTRACE_CODEGEN_VISITOR_H
#define EDUTRACE_CODEGEN_VISITOR_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "visitor.h"
#include "ast.h"

class CodeGenVisitor : public Visitor {
public:
    explicit CodeGenVisitor(std::ostream& out);
    void generate(Program& program);

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
    std::ostream& out;
    int labelCounter = 0;
    std::string currentEndLabel;
    std::unordered_map<std::string, int> offsets;
    std::unordered_map<std::string, Type> localTypes;
    std::unordered_map<std::string, StructDecl*> structDefs;
    std::unordered_map<std::string, FunctionDecl*> functionDefs;
    int nextOffset = 0;
    std::unordered_map<std::string, std::string> stringLabels;
    std::unordered_map<LambdaExpr*, std::string> lambdaLabels;
    std::vector<LambdaExpr*> pendingLambdas;
    std::vector<std::pair<FunctionDecl*, Type>> pendingTemplateSpecializations;
    std::unordered_set<std::string> emittedTemplateSpecializations;

    std::string newLabel(const std::string& base);
    int collectLocals(BlockStmt* block);
    void assignOffsets(FunctionDecl* fn);
    void assignOffsetsForCallable(const std::vector<Param>& params, BlockStmt* body);
    std::string labelForLambda(LambdaExpr* lambda);
    void emitPendingLambdas();
    void emitLambdaFunction(LambdaExpr* lambda, const std::string& label);
    std::string labelForTemplate(FunctionDecl* fn, const Type& concrete) const;
    std::string sanitizedTypeName(const Type& type) const;
    void queueTemplateSpecialization(FunctionDecl* fn, const Type& concrete);
    void emitTemplateSpecializations();
    void emitTemplateFunction(FunctionDecl* fn, const Type& concrete, const std::string& label);
    Type specializeType(const Type& type, const std::string& genericName, const Type& concrete) const;
    Type returnTypeForCall(const CallExpr* call) const;
    int slotsForType(const Type& type) const;
    std::vector<Param> fieldsForStruct(const Type& type) const;
    void emitExpr(Expr* expr);
    void emitLinearArrayIndex(const std::string& name, const std::vector<std::unique_ptr<Expr>>& indices, const SourceLocation& loc);
    Type typeAfterArrayAccess(const ArrayAccessExpr* access) const;
    bool isStringExpr(Expr* expr) const;
    bool isCharExpr(Expr* expr) const;
    std::string emitStringConstant(const std::string& value);
    std::string escapeString(const std::string& value);
};

#endif
