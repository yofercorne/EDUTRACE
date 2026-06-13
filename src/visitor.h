#ifndef EDUTRACE_VISITOR_H
#define EDUTRACE_VISITOR_H

class Program;
class StructDecl;
class FunctionDecl;
class BlockStmt;
class VarDeclStmt;
class AssignStmt;
class ArrayAssignStmt;
class PointerAssignStmt;
class FieldAssignStmt;
class DeleteStmt;
class PrintStmt;
class ReturnStmt;
class IfStmt;
class WhileStmt;
class TraceStmt;
class ExprStmt;
class BinaryExpr;
class UnaryExpr;
class IntExpr;
class BoolExpr;
class CharExpr;
class StringExpr;
class IdExpr;
class ArrayAccessExpr;
class FieldAccessExpr;
class NewExpr;
class LambdaExpr;
class CallExpr;

class Visitor {
public:
    virtual ~Visitor() = default;

    virtual void visit(Program* node) = 0;
    virtual void visit(StructDecl* node) = 0;
    virtual void visit(FunctionDecl* node) = 0;
    virtual void visit(BlockStmt* node) = 0;
    virtual void visit(VarDeclStmt* node) = 0;
    virtual void visit(AssignStmt* node) = 0;
    virtual void visit(ArrayAssignStmt* node) = 0;
    virtual void visit(PointerAssignStmt* node) = 0;
    virtual void visit(FieldAssignStmt* node) = 0;
    virtual void visit(DeleteStmt* node) = 0;
    virtual void visit(PrintStmt* node) = 0;
    virtual void visit(ReturnStmt* node) = 0;
    virtual void visit(IfStmt* node) = 0;
    virtual void visit(WhileStmt* node) = 0;
    virtual void visit(TraceStmt* node) = 0;
    virtual void visit(ExprStmt* node) = 0;

    virtual void visit(BinaryExpr* node) = 0;
    virtual void visit(UnaryExpr* node) = 0;
    virtual void visit(IntExpr* node) = 0;
    virtual void visit(BoolExpr* node) = 0;
    virtual void visit(CharExpr* node) = 0;
    virtual void visit(StringExpr* node) = 0;
    virtual void visit(IdExpr* node) = 0;
    virtual void visit(ArrayAccessExpr* node) = 0;
    virtual void visit(FieldAccessExpr* node) = 0;
    virtual void visit(NewExpr* node) = 0;
    virtual void visit(LambdaExpr* node) = 0;
    virtual void visit(CallExpr* node) = 0;
};

#endif
