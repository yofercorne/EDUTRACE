#ifndef EDUTRACE_TRACE_VISITOR_H
#define EDUTRACE_TRACE_VISITOR_H

#include <iostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "visitor.h"
#include "ast.h"

struct RuntimeValue {
    TypeKind kind = TypeKind::Unknown;
    long intValue = 0;
    bool boolValue = false;
    std::string stringValue;
    bool isArray = false;
    bool isStruct = false;
    bool isPointer = false;
    bool isLambda = false;
    LambdaExpr* lambda = nullptr;
    std::string lambdaName;
    std::string pointsTo;
    std::string structName;
    std::vector<int> dimensions;
    std::vector<RuntimeValue> elements;
    std::unordered_map<std::string, RuntimeValue> fields;

    static RuntimeValue Int(long v);
    static RuntimeValue Bool(bool v);
    static RuntimeValue Char(char v);
    static RuntimeValue String(std::string v);
    static RuntimeValue Pointer(std::string target);
    static RuntimeValue Lambda(LambdaExpr* lambda, std::string name = "");
    static RuntimeValue Array(TypeKind elementKind, const std::vector<int>& dimensions);
    static RuntimeValue Struct(std::string name);
    std::string toString() const;
    bool truthy() const;
};

class TraceVisitor : public Visitor {
public:
    explicit TraceVisitor(std::ostream& out = std::cout);
    void run(Program& program);

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
    Program* program = nullptr;
    std::vector<std::unordered_map<std::string, RuntimeValue>> envStack;
    std::vector<std::string> callStack;
    std::unordered_map<std::string, StructDecl*> structDefs;
    std::unordered_map<std::string, RuntimeValue> heapMemory;
    int nextHeapId = 1;
    RuntimeValue lastValue;
    bool returning = false;
    RuntimeValue returnValue;

    bool traceBranch = false;
    bool traceLoop = false;
    bool traceStack = false;
    bool traceRecursion = false;
    std::unordered_set<std::string> watchedVariables;
    std::unordered_set<std::string> watchedLoopVariables;
    std::unordered_set<std::string> watchedArrays;
    std::unordered_set<std::string> watchedMemoryNames;

    RuntimeValue eval(Expr* expr);
    RuntimeValue callFunction(FunctionDecl* fn, const std::vector<RuntimeValue>& args);
    RuntimeValue callLambda(LambdaExpr* lambda, const std::string& name, const std::vector<RuntimeValue>& args);
    RuntimeValue getVar(const std::string& name, const SourceLocation& loc);
    void setVar(const std::string& name, const RuntimeValue& value, const SourceLocation& loc);
    void setPathValue(const std::string& name, const RuntimeValue& value, const SourceLocation& loc);
    void declareVar(const std::string& name, const RuntimeValue& value);
    RuntimeValue defaultValueForType(const Type& type) const;
    RuntimeValue getFieldValue(const std::string& objectName, const std::string& fieldName, const SourceLocation& loc);
    void setFieldValue(const std::string& objectName, const std::string& fieldName, const RuntimeValue& value, const SourceLocation& loc);
    RuntimeValue getPathValue(const std::string& name, const SourceLocation& loc);
    RuntimeValue getArrayElement(const std::string& name, const std::vector<long>& indices, const SourceLocation& loc);
    void setArrayElement(const std::string& name, const std::vector<long>& indices, const RuntimeValue& value, const SourceLocation& loc);
    size_t linearArrayIndex(const RuntimeValue& arrayValue, const std::vector<long>& indices, const std::string& name, const SourceLocation& loc) const;
    void printArray(const std::string& name, const std::string& title);
    void printMemoryState(const std::vector<std::string>& names, const std::string& title);
    void printWatchedMemoryState(const std::string& title);
    void printWatchedVariables(const std::unordered_set<std::string>& names, const std::string& title);
    void printCurrentStackFrame(const std::string& title);
    void printCallStack(const std::string& title);
    std::string exprToString(const Expr* expr) const;
    std::string indent() const;
};

#endif
