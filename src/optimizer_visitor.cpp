#include "optimizer_visitor.h"
#include <memory>
#include <ostream>
#include <utility>

namespace {

bool numericConst(const Expr* expr, long& value) {
    if (auto* i = dynamic_cast<const IntExpr*>(expr)) {
        value = i->value;
        return true;
    }
    if (auto* c = dynamic_cast<const CharExpr*>(expr)) {
        value = static_cast<unsigned char>(c->value);
        return true;
    }
    if (auto* b = dynamic_cast<const BoolExpr*>(expr)) {
        value = b->value ? 1 : 0;
        return true;
    }
    return false;
}

bool boolConst(const Expr* expr, bool& value) {
    if (auto* b = dynamic_cast<const BoolExpr*>(expr)) {
        value = b->value;
        return true;
    }
    return false;
}

std::unique_ptr<Expr> makeInt(long value, SourceLocation loc) {
    return std::make_unique<IntExpr>(value, loc);
}

std::unique_ptr<Expr> makeBool(bool value, SourceLocation loc) {
    return std::make_unique<BoolExpr>(value, loc);
}

} // namespace

void OptimizerVisitor::optimize(Program& program) {
    stats = OptimizationStats{};
    for (auto& fn : program.functions) optimizeBlock(fn->body.get());
}

void OptimizerVisitor::printReport(std::ostream& out) const {
    out << "Resumen de optimizaciones\n";
    out << "  Constant folding: " << stats.constantFolds << "\n";
    out << "  Simplificaciones algebraicas: " << stats.algebraicSimplifications << "\n";
    out << "  Código muerto eliminado: " << stats.deadStatementsRemoved << "\n";
    out << "  If con condición constante simplificados: " << stats.constantBranchesSimplified << "\n";
    out << "  While(false) eliminados: " << stats.constantLoopsRemoved << "\n";
    out << "  Total: " << stats.total() << "\n";
}

void OptimizerVisitor::optimizeBlock(BlockStmt* block) {
    bool seenReturn = false;
    std::vector<std::unique_ptr<Stmt>> kept;

    auto appendOptimizedStatements = [&](std::vector<std::unique_ptr<Stmt>>& statements) {
        for (auto& inner : statements) {
            if (seenReturn) {
                stats.deadStatementsRemoved++;
                continue;
            }
            if (dynamic_cast<ReturnStmt*>(inner.get())) seenReturn = true;
            kept.push_back(std::move(inner));
        }
    };

    for (auto& stmt : block->statements) {
        if (seenReturn) {
            stats.deadStatementsRemoved++;
            continue;
        }

        optimizeStmt(stmt.get());

        if (auto* ifs = dynamic_cast<IfStmt*>(stmt.get())) {
            bool condValue = false;
            if (boolConst(ifs->condition.get(), condValue)) {
                stats.constantBranchesSimplified++;
                if (condValue) {
                    appendOptimizedStatements(ifs->thenBranch->statements);
                } else if (ifs->elseBranch) {
                    appendOptimizedStatements(ifs->elseBranch->statements);
                }
                continue;
            }
        }

        if (auto* wh = dynamic_cast<WhileStmt*>(stmt.get())) {
            bool condValue = false;
            if (boolConst(wh->condition.get(), condValue) && !condValue) {
                stats.constantLoopsRemoved++;
                continue;
            }
        }

        if (dynamic_cast<ReturnStmt*>(stmt.get())) seenReturn = true;
        kept.push_back(std::move(stmt));
    }
    block->statements = std::move(kept);
}

void OptimizerVisitor::optimizeStmt(Stmt* stmt) {
    if (auto* v = dynamic_cast<VarDeclStmt*>(stmt)) {
        if (v->initializer) optimizeExpr(v->initializer);
    } else if (auto* a = dynamic_cast<AssignStmt*>(stmt)) {
        optimizeExpr(a->value);
    } else if (auto* aa = dynamic_cast<ArrayAssignStmt*>(stmt)) {
        for (auto& idx : aa->indices) optimizeExpr(idx);
        optimizeExpr(aa->value);
    } else if (auto* pa = dynamic_cast<PointerAssignStmt*>(stmt)) {
        optimizeExpr(pa->value);
    } else if (auto* fa = dynamic_cast<FieldAssignStmt*>(stmt)) {
        optimizeExpr(fa->value);
    } else if (dynamic_cast<DeleteStmt*>(stmt)) {
        // delete no contiene subexpresiones en esta versión.
    } else if (auto* p = dynamic_cast<PrintStmt*>(stmt)) {
        optimizeExpr(p->expression);
    } else if (auto* r = dynamic_cast<ReturnStmt*>(stmt)) {
        if (r->value) optimizeExpr(r->value);
    } else if (auto* i = dynamic_cast<IfStmt*>(stmt)) {
        optimizeExpr(i->condition);
        optimizeBlock(i->thenBranch.get());
        if (i->elseBranch) optimizeBlock(i->elseBranch.get());
    } else if (auto* w = dynamic_cast<WhileStmt*>(stmt)) {
        optimizeExpr(w->condition);
        optimizeBlock(w->body.get());
    } else if (auto* e = dynamic_cast<ExprStmt*>(stmt)) {
        optimizeExpr(e->expression);
    } else if (auto* b = dynamic_cast<BlockStmt*>(stmt)) {
        optimizeBlock(b);
    }
}

void OptimizerVisitor::optimizeExpr(std::unique_ptr<Expr>& expr) {
    if (auto* b = dynamic_cast<BinaryExpr*>(expr.get())) {
        optimizeExpr(b->left);
        optimizeExpr(b->right);

        long leftNum = 0;
        long rightNum = 0;
        bool hasLeftNum = numericConst(b->left.get(), leftNum);
        bool hasRightNum = numericConst(b->right.get(), rightNum);

        bool leftBool = false;
        bool rightBool = false;
        bool hasLeftBool = boolConst(b->left.get(), leftBool);
        bool hasRightBool = boolConst(b->right.get(), rightBool);

        if (hasLeftNum && hasRightNum) {
            if (b->op == "+") { expr = makeInt(leftNum + rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "-") { expr = makeInt(leftNum - rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "*") { expr = makeInt(leftNum * rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "/" && rightNum != 0) { expr = makeInt(leftNum / rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "%" && rightNum != 0) { expr = makeInt(leftNum % rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "==") { expr = makeBool(leftNum == rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "!=") { expr = makeBool(leftNum != rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "<") { expr = makeBool(leftNum < rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == "<=") { expr = makeBool(leftNum <= rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == ">") { expr = makeBool(leftNum > rightNum, b->loc); stats.constantFolds++; return; }
            if (b->op == ">=") { expr = makeBool(leftNum >= rightNum, b->loc); stats.constantFolds++; return; }
        }

        if (hasLeftBool && hasRightBool) {
            if (b->op == "&&") { expr = makeBool(leftBool && rightBool, b->loc); stats.constantFolds++; return; }
            if (b->op == "||") { expr = makeBool(leftBool || rightBool, b->loc); stats.constantFolds++; return; }
            if (b->op == "==") { expr = makeBool(leftBool == rightBool, b->loc); stats.constantFolds++; return; }
            if (b->op == "!=") { expr = makeBool(leftBool != rightBool, b->loc); stats.constantFolds++; return; }
        }

        // Simplificaciones booleanas: true && x -> x, false && x -> false, etc.
        if (b->op == "&&") {
            if (hasLeftBool) {
                expr = leftBool ? std::move(b->right) : makeBool(false, b->loc);
                stats.algebraicSimplifications++;
                return;
            }
            if (hasRightBool) {
                expr = rightBool ? std::move(b->left) : makeBool(false, b->loc);
                stats.algebraicSimplifications++;
                return;
            }
        }
        if (b->op == "||") {
            if (hasLeftBool) {
                expr = leftBool ? makeBool(true, b->loc) : std::move(b->right);
                stats.algebraicSimplifications++;
                return;
            }
            if (hasRightBool) {
                expr = rightBool ? makeBool(true, b->loc) : std::move(b->left);
                stats.algebraicSimplifications++;
                return;
            }
        }

        // Simplificaciones algebraicas aritméticas.
        if (hasRightNum) {
            if ((b->op == "+" || b->op == "-") && rightNum == 0) {
                expr = std::move(b->left);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "*" && rightNum == 1) {
                expr = std::move(b->left);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "*" && rightNum == 0) {
                expr = makeInt(0, b->loc);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "/" && rightNum == 1) {
                expr = std::move(b->left);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "%" && rightNum == 1) {
                expr = makeInt(0, b->loc);
                stats.algebraicSimplifications++;
                return;
            }
        }
        if (hasLeftNum) {
            if (b->op == "+" && leftNum == 0) {
                expr = std::move(b->right);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "*" && leftNum == 1) {
                expr = std::move(b->right);
                stats.algebraicSimplifications++;
                return;
            }
            if (b->op == "*" && leftNum == 0) {
                expr = makeInt(0, b->loc);
                stats.algebraicSimplifications++;
                return;
            }
        }
    } else if (auto* u = dynamic_cast<UnaryExpr*>(expr.get())) {
        optimizeExpr(u->right);
        long num = 0;
        bool boolValue = false;
        if (u->op == "-" && numericConst(u->right.get(), num)) {
            expr = makeInt(-num, u->loc);
            stats.constantFolds++;
            return;
        }
        if (u->op == "!" && boolConst(u->right.get(), boolValue)) {
            expr = makeBool(!boolValue, u->loc);
            stats.constantFolds++;
            return;
        }
    } else if (auto* a = dynamic_cast<ArrayAccessExpr*>(expr.get())) {
        for (auto& idx : a->indices) optimizeExpr(idx);
    } else if (dynamic_cast<FieldAccessExpr*>(expr.get())) {
        // No requiere optimización interna.
    } else if (auto* l = dynamic_cast<LambdaExpr*>(expr.get())) {
        optimizeBlock(l->body.get());
    } else if (auto* c = dynamic_cast<CallExpr*>(expr.get())) {
        for (auto& arg : c->arguments) optimizeExpr(arg);
    }
}

void OptimizerVisitor::visit(Program*) {}
void OptimizerVisitor::visit(StructDecl*) {}
void OptimizerVisitor::visit(FunctionDecl*) {}
void OptimizerVisitor::visit(BlockStmt*) {}
void OptimizerVisitor::visit(VarDeclStmt*) {}
void OptimizerVisitor::visit(AssignStmt*) {}
void OptimizerVisitor::visit(ArrayAssignStmt*) {}
void OptimizerVisitor::visit(PointerAssignStmt*) {}
void OptimizerVisitor::visit(FieldAssignStmt*) {}
void OptimizerVisitor::visit(DeleteStmt*) {}
void OptimizerVisitor::visit(PrintStmt*) {}
void OptimizerVisitor::visit(ReturnStmt*) {}
void OptimizerVisitor::visit(IfStmt*) {}
void OptimizerVisitor::visit(WhileStmt*) {}
void OptimizerVisitor::visit(TraceStmt*) {}
void OptimizerVisitor::visit(ExprStmt*) {}
void OptimizerVisitor::visit(BinaryExpr*) {}
void OptimizerVisitor::visit(UnaryExpr*) {}
void OptimizerVisitor::visit(IntExpr*) {}
void OptimizerVisitor::visit(BoolExpr*) {}
void OptimizerVisitor::visit(CharExpr*) {}
void OptimizerVisitor::visit(StringExpr*) {}
void OptimizerVisitor::visit(IdExpr*) {}
void OptimizerVisitor::visit(ArrayAccessExpr*) {}
void OptimizerVisitor::visit(FieldAccessExpr*) {}
void OptimizerVisitor::visit(NewExpr*) {}
void OptimizerVisitor::visit(LambdaExpr*) {}
void OptimizerVisitor::visit(CallExpr*) {}
