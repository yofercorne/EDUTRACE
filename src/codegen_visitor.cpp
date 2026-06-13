#include "codegen_visitor.h"
#include "errors.h"
#include <algorithm>
#include <sstream>
#include <functional>
#include <cctype>

CodeGenVisitor::CodeGenVisitor(std::ostream& o) : out(o) {}

void CodeGenVisitor::generate(Program& program) {
    program.accept(*this);
}

std::string CodeGenVisitor::newLabel(const std::string& base) {
    return base + "_" + std::to_string(labelCounter++);
}

void CodeGenVisitor::visit(Program* node) {
    structDefs.clear();
    functionDefs.clear();
    emittedTemplateSpecializations.clear();
    pendingTemplateSpecializations.clear();
    for (auto& st : node->structs) structDefs[st->name] = st.get();
    for (auto& fn : node->functions) functionDefs[fn->name] = fn.get();
    out << ".section .rodata\n";
    out << "fmt_int: .string \"%ld\\n\"\n";
    out << "fmt_char: .string \"%c\\n\"\n";
    out << "fmt_str: .string \"%s\\n\"\n";
    out << ".text\n";
    out << ".extern printf\n";
    out << ".extern malloc\n";
    out << ".extern free\n";
    for (auto& fn : node->functions) {
        if (!fn->isTemplate()) fn->accept(*this);
    }
    emitTemplateSpecializations();
}

int CodeGenVisitor::slotsForType(const Type& type) const {
    if (type.kind == TypeKind::Struct && !type.isArray()) {
        auto it = structDefs.find(type.structName);
        if (it != structDefs.end()) return static_cast<int>(std::max<size_t>(1, it->second->fields.size()));
    }
    return static_cast<int>(std::max<long>(1, type.totalElements()));
}

std::vector<Param> CodeGenVisitor::fieldsForStruct(const Type& type) const {
    auto it = structDefs.find(type.structName);
    if (it == structDefs.end()) return {};
    return it->second->fields;
}

int CodeGenVisitor::collectLocals(BlockStmt* block) {
    int count = 0;
    for (auto& stmt : block->statements) {
        if (dynamic_cast<VarDeclStmt*>(stmt.get())) count++;
        else if (auto* ifs = dynamic_cast<IfStmt*>(stmt.get())) {
            count += collectLocals(ifs->thenBranch.get());
            if (ifs->elseBranch) count += collectLocals(ifs->elseBranch.get());
        } else if (auto* wh = dynamic_cast<WhileStmt*>(stmt.get())) {
            count += collectLocals(wh->body.get());
        } else if (auto* b = dynamic_cast<BlockStmt*>(stmt.get())) {
            count += collectLocals(b);
        }
    }
    return count;
}

void CodeGenVisitor::assignOffsets(FunctionDecl* fn) {
    assignOffsetsForCallable(fn->params, fn->body.get());
}

void CodeGenVisitor::assignOffsetsForCallable(const std::vector<Param>& params, BlockStmt* body) {
    offsets.clear();
    localTypes.clear();
    nextOffset = 0;
    auto allocate = [&](const std::string& name, const Type& type) {
        localTypes[name] = type;
        if (type.kind == TypeKind::Struct && !type.isArray()) {
            for (const auto& field : fieldsForStruct(type)) {
                nextOffset += 8 * slotsForType(field.type);
                offsets[name + "." + field.name] = -nextOffset;
                localTypes[name + "." + field.name] = field.type;
            }
        } else {
            nextOffset += 8 * slotsForType(type);
            offsets[name] = -nextOffset;
        }
    };

    for (const auto& p : params) {
        allocate(p.name, p.type);
    }
    std::function<void(BlockStmt*)> walk = [&](BlockStmt* block) {
        for (auto& stmt : block->statements) {
            if (auto* v = dynamic_cast<VarDeclStmt*>(stmt.get())) {
                allocate(v->name, v->type);
            } else if (auto* ifs = dynamic_cast<IfStmt*>(stmt.get())) {
                walk(ifs->thenBranch.get());
                if (ifs->elseBranch) walk(ifs->elseBranch.get());
            } else if (auto* wh = dynamic_cast<WhileStmt*>(stmt.get())) {
                walk(wh->body.get());
            } else if (auto* b = dynamic_cast<BlockStmt*>(stmt.get())) {
                walk(b);
            }
        }
    };
    walk(body);
}


std::string CodeGenVisitor::labelForLambda(LambdaExpr* lambda) {
    auto it = lambdaLabels.find(lambda);
    if (it != lambdaLabels.end()) return it->second;
    std::string label = ".Llambda_" + std::to_string(labelCounter++);
    lambdaLabels[lambda] = label;
    pendingLambdas.push_back(lambda);
    return label;
}

void CodeGenVisitor::emitPendingLambdas() {
    auto lambdas = pendingLambdas;
    pendingLambdas.clear();
    for (auto* lambda : lambdas) {
        emitLambdaFunction(lambda, lambdaLabels[lambda]);
    }
}

void CodeGenVisitor::emitLambdaFunction(LambdaExpr* lambda, const std::string& label) {
    auto savedOffsets = offsets;
    auto savedLocalTypes = localTypes;
    int savedNextOffset = nextOffset;
    std::string savedEndLabel = currentEndLabel;

    assignOffsetsForCallable(lambda->params, lambda->body.get());
    int localBytes = nextOffset;
    int alignedBytes = ((localBytes + 15) / 16) * 16;
    currentEndLabel = ".Lend" + label.substr(1);

    out << label << ":\n";
    out << "  pushq %rbp\n";
    out << "  movq %rsp, %rbp\n";
    if (alignedBytes > 0) out << "  subq $" << alignedBytes << ", %rsp\n";

    static const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    for (size_t i = 0; i < lambda->params.size() && i < 6; ++i) {
        out << "  movq " << regs[i] << ", " << offsets[lambda->params[i].name] << "(%rbp)\n";
    }

    for (auto& stmt : lambda->body->statements) stmt->accept(*this);

    out << currentEndLabel << ":\n";
    out << "  leave\n";
    out << "  ret\n";

    offsets = savedOffsets;
    localTypes = savedLocalTypes;
    nextOffset = savedNextOffset;
    currentEndLabel = savedEndLabel;
}


std::string CodeGenVisitor::sanitizedTypeName(const Type& type) const {
    std::string raw = type.toString();
    std::string outName;
    for (char c : raw) {
        if (std::isalnum(static_cast<unsigned char>(c))) outName += c;
        else outName += '_';
    }
    return outName;
}

std::string CodeGenVisitor::labelForTemplate(FunctionDecl* fn, const Type& concrete) const {
    return fn->name + "__" + sanitizedTypeName(concrete);
}

void CodeGenVisitor::queueTemplateSpecialization(FunctionDecl* fn, const Type& concrete) {
    std::string label = labelForTemplate(fn, concrete);
    if (emittedTemplateSpecializations.count(label)) return;
    emittedTemplateSpecializations.insert(label);
    pendingTemplateSpecializations.push_back({fn, concrete});
}

Type CodeGenVisitor::specializeType(const Type& type, const std::string& genericName, const Type& concrete) const {
    if (type.kind == TypeKind::Generic && type.genericName == genericName && type.pointerDepth == 0 && !type.isArray()) return concrete;
    Type result = type;
    if (type.kind == TypeKind::Generic && type.genericName == genericName) {
        result = concrete;
        result.pointerDepth += type.pointerDepth;
        result.dimensions.insert(result.dimensions.end(), type.dimensions.begin(), type.dimensions.end());
        return result;
    }
    if (type.kind == TypeKind::Lambda) {
        for (auto& param : result.lambdaParams) param = specializeType(param, genericName, concrete);
        if (result.lambdaReturn) result.lambdaReturn = new Type(specializeType(*result.lambdaReturn, genericName, concrete));
    }
    return result;
}

void CodeGenVisitor::emitTemplateSpecializations() {
    while (!pendingTemplateSpecializations.empty()) {
        auto current = pendingTemplateSpecializations;
        pendingTemplateSpecializations.clear();
        for (auto& item : current) {
            emitTemplateFunction(item.first, item.second, labelForTemplate(item.first, item.second));
        }
    }
}

void CodeGenVisitor::emitTemplateFunction(FunctionDecl* fn, const Type& concrete, const std::string& label) {
    auto savedOffsets = offsets;
    auto savedLocalTypes = localTypes;
    int savedNextOffset = nextOffset;
    std::string savedEndLabel = currentEndLabel;

    std::vector<Param> params = fn->params;
    for (auto& param : params) param.type = specializeType(param.type, fn->templateParam, concrete);
    assignOffsetsForCallable(params, fn->body.get());
    int localBytes = nextOffset;
    int alignedBytes = ((localBytes + 15) / 16) * 16;
    currentEndLabel = ".Lend_" + label;

    out << label << ":\n";
    out << "  pushq %rbp\n";
    out << "  movq %rsp, %rbp\n";
    if (alignedBytes > 0) out << "  subq $" << alignedBytes << ", %rsp\n";

    static const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    for (size_t i = 0; i < params.size() && i < 6; ++i) {
        out << "  movq " << regs[i] << ", " << offsets[params[i].name] << "(%rbp)\n";
    }

    for (auto& stmt : fn->body->statements) stmt->accept(*this);

    out << currentEndLabel << ":\n";
    out << "  leave\n";
    out << "  ret\n";

    offsets = savedOffsets;
    localTypes = savedLocalTypes;
    nextOffset = savedNextOffset;
    currentEndLabel = savedEndLabel;
}

Type CodeGenVisitor::returnTypeForCall(const CallExpr* call) const {
    auto fnIt = functionDefs.find(call->callee);
    if (fnIt != functionDefs.end()) {
        FunctionDecl* fn = fnIt->second;
        if (fn->isTemplate() && call->typeArguments.size() == 1) return specializeType(fn->returnType, fn->templateParam, call->typeArguments[0]);
        return fn->returnType;
    }
    auto it = localTypes.find(call->callee);
    if (it != localTypes.end() && it->second.kind == TypeKind::Lambda && it->second.lambdaReturn) return *it->second.lambdaReturn;
    return Type::Unknown();
}

void CodeGenVisitor::visit(StructDecl*) {}

void CodeGenVisitor::visit(FunctionDecl* node) {
    pendingLambdas.clear();
    assignOffsets(node);
    int localBytes = nextOffset;
    int alignedBytes = ((localBytes + 15) / 16) * 16;
    currentEndLabel = ".Lend_" + node->name;

    out << ".globl " << node->name << "\n";
    out << node->name << ":\n";
    out << "  pushq %rbp\n";
    out << "  movq %rsp, %rbp\n";
    if (alignedBytes > 0) out << "  subq $" << alignedBytes << ", %rsp\n";

    static const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    for (size_t i = 0; i < node->params.size() && i < 6; ++i) {
        if (node->params[i].type.kind == TypeKind::Struct && !node->params[i].type.isArray()) {
            throw SemanticError(node->params[i].loc, "codegen aún no soporta structs como parámetros");
        }
        out << "  movq " << regs[i] << ", " << offsets[node->params[i].name] << "(%rbp)\n";
    }

    for (auto& stmt : node->body->statements) stmt->accept(*this);

    out << currentEndLabel << ":\n";
    out << "  leave\n";
    out << "  ret\n";

    emitPendingLambdas();
}

void CodeGenVisitor::visit(BlockStmt* node) {
    for (auto& stmt : node->statements) stmt->accept(*this);
}

void CodeGenVisitor::visit(VarDeclStmt* node) {
    if (node->type.kind == TypeKind::Struct && !node->type.isArray()) {
        if (node->initializer) throw SemanticError(node->loc, "codegen aún no soporta inicialización directa de structs");
        for (const auto& field : fieldsForStruct(node->type)) {
            auto fieldIt = offsets.find(node->name + "." + field.name);
            if (fieldIt == offsets.end()) throw SemanticError(node->loc, "campo sin offset en codegen: " + node->name + "." + field.name);
            if (field.type.kind == TypeKind::String && field.type.pointerDepth == 0) {
                std::string empty = emitStringConstant("");
                out << "  leaq " << empty << "(%rip), %rax\n";
                out << "  movq %rax, " << fieldIt->second << "(%rbp)\n";
            } else {
                out << "  movq $0, " << fieldIt->second << "(%rbp)\n";
            }
        }
        return;
    }

    auto it = offsets.find(node->name);
    if (it == offsets.end()) throw SemanticError(node->loc, "variable sin offset en codegen: " + node->name);

    if (node->initializer) {
        emitExpr(node->initializer.get());
        out << "  movq %rax, " << it->second << "(%rbp)\n";
        return;
    }

    if (node->type.kind == TypeKind::String && node->type.pointerDepth == 0) {
        std::string empty = emitStringConstant("");
        if (node->type.isArray()) {
            for (long i = 0; i < node->type.totalElements(); ++i) {
                out << "  leaq " << empty << "(%rip), %rax\n";
                out << "  movq %rax, " << (it->second + static_cast<int>(i * 8)) << "(%rbp)\n";
            }
        } else {
            out << "  leaq " << empty << "(%rip), %rax\n";
            out << "  movq %rax, " << it->second << "(%rbp)\n";
        }
        return;
    }

    for (long i = 0; i < node->type.totalElements(); ++i) {
        out << "  movq $0, " << (it->second + static_cast<int>(i * 8)) << "(%rbp)\n";
    }
}

void CodeGenVisitor::visit(AssignStmt* node) {
    emitExpr(node->value.get());
    auto it = offsets.find(node->name);
    if (it == offsets.end()) throw SemanticError(node->loc, "variable sin offset en codegen: " + node->name);
    out << "  movq %rax, " << it->second << "(%rbp)\n";
}

void CodeGenVisitor::visit(ArrayAssignStmt* node) {
    auto it = offsets.find(node->name);
    if (it == offsets.end()) throw SemanticError(node->loc, "arreglo sin offset en codegen: " + node->name);
    emitLinearArrayIndex(node->name, node->indices, node->loc);
    out << "  pushq %rax\n";
    emitExpr(node->value.get());
    out << "  movq %rax, %rcx\n";
    out << "  popq %rax\n";
    out << "  movq %rcx, " << it->second << "(%rbp,%rax,8)\n";
}

void CodeGenVisitor::visit(PointerAssignStmt* node) {
    auto it = offsets.find(node->pointerName);
    if (it == offsets.end()) throw SemanticError(node->loc, "puntero sin offset en codegen: " + node->pointerName);
    emitExpr(node->value.get());
    out << "  pushq %rax\n";
    out << "  movq " << it->second << "(%rbp), %rax\n";
    out << "  popq %rcx\n";
    out << "  movq %rcx, (%rax)\n";
}

void CodeGenVisitor::visit(FieldAssignStmt* node) {
    emitExpr(node->value.get());
    auto it = offsets.find(node->fullName());
    if (it == offsets.end()) throw SemanticError(node->loc, "campo sin offset en codegen: " + node->fullName());
    out << "  movq %rax, " << it->second << "(%rbp)\n";
}

void CodeGenVisitor::visit(DeleteStmt* node) {
    auto it = offsets.find(node->pointerName);
    if (it == offsets.end()) throw SemanticError(node->loc, "puntero sin offset en delete: " + node->pointerName);
    out << "  movq " << it->second << "(%rbp), %rdi\n";
    out << "  call free@PLT\n";
    out << "  movq $0, " << it->second << "(%rbp)\n";
}

void CodeGenVisitor::visit(PrintStmt* node) {
    bool printsString = isStringExpr(node->expression.get());
    bool printsChar = isCharExpr(node->expression.get());
    emitExpr(node->expression.get());
    out << "  movq %rax, %rsi\n";
    const char* fmt = printsString ? "fmt_str" : (printsChar ? "fmt_char" : "fmt_int");
    out << "  leaq " << fmt << "(%rip), %rdi\n";
    out << "  movq $0, %rax\n";
    out << "  call printf@PLT\n";
}

void CodeGenVisitor::visit(ReturnStmt* node) {
    if (node->value) emitExpr(node->value.get());
    else out << "  movq $0, %rax\n";
    out << "  jmp " << currentEndLabel << "\n";
}

void CodeGenVisitor::visit(IfStmt* node) {
    std::string elseLabel = newLabel(".Lelse");
    std::string endLabel = newLabel(".Lendif");
    emitExpr(node->condition.get());
    out << "  cmpq $0, %rax\n";
    out << "  je " << elseLabel << "\n";
    node->thenBranch->accept(*this);
    out << "  jmp " << endLabel << "\n";
    out << elseLabel << ":\n";
    if (node->elseBranch) node->elseBranch->accept(*this);
    out << endLabel << ":\n";
}

void CodeGenVisitor::visit(WhileStmt* node) {
    std::string startLabel = newLabel(".Lwhile_start");
    std::string endLabel = newLabel(".Lwhile_end");
    out << startLabel << ":\n";
    emitExpr(node->condition.get());
    out << "  cmpq $0, %rax\n";
    out << "  je " << endLabel << "\n";
    node->body->accept(*this);
    out << "  jmp " << startLabel << "\n";
    out << endLabel << ":\n";
}

void CodeGenVisitor::visit(TraceStmt* node) {
    out << "  # trace " << traceKindName(node->kind) << " omitido en modo asm; usar --trace para salida educativa\n";
}

void CodeGenVisitor::visit(ExprStmt* node) { emitExpr(node->expression.get()); }
void CodeGenVisitor::emitExpr(Expr* expr) { expr->accept(*this); }

void CodeGenVisitor::visit(IntExpr* node) { out << "  movq $" << node->value << ", %rax\n"; }
void CodeGenVisitor::visit(BoolExpr* node) { out << "  movq $" << (node->value ? 1 : 0) << ", %rax\n"; }
void CodeGenVisitor::visit(CharExpr* node) { out << "  movq $" << static_cast<int>(static_cast<unsigned char>(node->value)) << ", %rax\n"; }
void CodeGenVisitor::visit(StringExpr* node) {
    std::string label = emitStringConstant(node->value);
    out << "  leaq " << label << "(%rip), %rax\n";
}

void CodeGenVisitor::visit(IdExpr* node) {
    auto it = offsets.find(node->name);
    if (it == offsets.end()) throw SemanticError(node->loc, "variable sin offset en codegen: " + node->name);
    auto typeIt = localTypes.find(node->name);
    if (typeIt != localTypes.end() && typeIt->second.isArray()) {
        throw SemanticError(node->loc, "no se puede usar arreglo completo como expresión: " + node->name);
    }
    out << "  movq " << it->second << "(%rbp), %rax\n";
}

void CodeGenVisitor::visit(ArrayAccessExpr* node) {
    auto it = offsets.find(node->name);
    if (it == offsets.end()) throw SemanticError(node->loc, "arreglo sin offset en codegen: " + node->name);
    Type resultType = typeAfterArrayAccess(node);
    if (resultType.isArray()) {
        throw SemanticError(node->loc, "codegen requiere acceso completo al arreglo '" + node->name + "'");
    }
    emitLinearArrayIndex(node->name, node->indices, node->loc);
    out << "  movq " << it->second << "(%rbp,%rax,8), %rax\n";
}

void CodeGenVisitor::visit(FieldAccessExpr* node) {
    auto it = offsets.find(node->fullName());
    if (it == offsets.end()) throw SemanticError(node->loc, "campo sin offset en codegen: " + node->fullName());
    out << "  movq " << it->second << "(%rbp), %rax\n";
}

void CodeGenVisitor::visit(UnaryExpr* node) {
    if (node->op == "&") {
        auto* id = dynamic_cast<IdExpr*>(node->right.get());
        if (!id) throw SemanticError(node->loc, "codegen solo soporta & sobre variables simples");
        auto it = offsets.find(id->name);
        if (it == offsets.end()) throw SemanticError(node->loc, "variable sin offset para &: " + id->name);
        out << "  leaq " << it->second << "(%rbp), %rax\n";
        return;
    }

    emitExpr(node->right.get());
    if (node->op == "-") out << "  negq %rax\n";
    else if (node->op == "!") {
        out << "  cmpq $0, %rax\n";
        out << "  movq $0, %rax\n";
        out << "  sete %al\n";
        out << "  movzbq %al, %rax\n";
    } else if (node->op == "*") {
        out << "  movq (%rax), %rax\n";
    }
}

void CodeGenVisitor::visit(BinaryExpr* node) {
    emitExpr(node->left.get());
    out << "  pushq %rax\n";
    emitExpr(node->right.get());
    out << "  movq %rax, %rcx\n";
    out << "  popq %rax\n";

    const std::string& op = node->op;
    if (op == "+") out << "  addq %rcx, %rax\n";
    else if (op == "-") out << "  subq %rcx, %rax\n";
    else if (op == "*") out << "  imulq %rcx, %rax\n";
    else if (op == "/" || op == "%") {
        out << "  cqto\n";
        out << "  idivq %rcx\n";
        if (op == "%") out << "  movq %rdx, %rax\n";
    } else if (op == "&&") {
        out << "  cmpq $0, %rax\n";
        out << "  setne %al\n";
        out << "  movzbq %al, %rax\n";
        out << "  cmpq $0, %rcx\n";
        out << "  setne %cl\n";
        out << "  movzbq %cl, %rcx\n";
        out << "  andq %rcx, %rax\n";
    } else if (op == "||") {
        out << "  orq %rcx, %rax\n";
        out << "  cmpq $0, %rax\n";
        out << "  movq $0, %rax\n";
        out << "  setne %al\n";
        out << "  movzbq %al, %rax\n";
    } else {
        out << "  cmpq %rcx, %rax\n";
        out << "  movq $0, %rax\n";
        if (op == "==") out << "  sete %al\n";
        else if (op == "!=") out << "  setne %al\n";
        else if (op == "<") out << "  setl %al\n";
        else if (op == "<=") out << "  setle %al\n";
        else if (op == ">") out << "  setg %al\n";
        else if (op == ">=") out << "  setge %al\n";
        out << "  movzbq %al, %rax\n";
    }
}

void CodeGenVisitor::visit(NewExpr* node) {
    if (node->allocatedType.kind != TypeKind::Int || node->allocatedType.pointerDepth != 0 || node->allocatedType.isArray()) {
        throw SemanticError(node->loc, "codegen solo soporta new int en esta versión");
    }
    out << "  movq $8, %rdi\n";
    out << "  call malloc@PLT\n";
    out << "  movq $0, (%rax)\n";
}

void CodeGenVisitor::visit(LambdaExpr* node) {
    std::string label = labelForLambda(node);
    out << "  leaq " << label << "(%rip), %rax\n";
}

void CodeGenVisitor::visit(CallExpr* node) {
    static const char* regs[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
    if (node->arguments.size() > 6) throw SemanticError(node->loc, "codegen soporta máximo 6 argumentos por ahora");
    for (auto& arg : node->arguments) {
        emitExpr(arg.get());
        out << "  pushq %rax\n";
    }
    for (int i = static_cast<int>(node->arguments.size()) - 1; i >= 0; --i) {
        out << "  popq " << regs[i] << "\n";
    }

    auto typeIt = localTypes.find(node->callee);
    if (typeIt != localTypes.end() && typeIt->second.kind == TypeKind::Lambda) {
        auto off = offsets.find(node->callee);
        if (off == offsets.end()) throw SemanticError(node->loc, "lambda sin offset en codegen: " + node->callee);
        out << "  movq " << off->second << "(%rbp), %rax\n";
        out << "  call *%rax\n";
        return;
    }

    auto fnIt = functionDefs.find(node->callee);
    if (fnIt != functionDefs.end() && fnIt->second->isTemplate()) {
        if (node->typeArguments.size() != 1) throw SemanticError(node->loc, "codegen requiere especialización explícita para template: " + node->callee);
        FunctionDecl* fn = fnIt->second;
        queueTemplateSpecialization(fn, node->typeArguments[0]);
        out << "  call " << labelForTemplate(fn, node->typeArguments[0]) << "\n";
        return;
    }

    out << "  call " << node->callee << "\n";
}

void CodeGenVisitor::emitLinearArrayIndex(const std::string& name, const std::vector<std::unique_ptr<Expr>>& indices, const SourceLocation& loc) {
    auto typeIt = localTypes.find(name);
    if (typeIt == localTypes.end() || !typeIt->second.isArray()) {
        throw SemanticError(loc, "no se encontró tipo de arreglo en codegen: " + name);
    }
    const auto& dims = typeIt->second.dimensions;
    if (indices.empty() || indices.size() > dims.size()) {
        throw SemanticError(loc, "cantidad de índices inválida para arreglo '" + name + "'");
    }

    bool first = true;
    for (size_t i = 0; i < indices.size(); ++i) {
        emitExpr(indices[i].get());
        long stride = 1;
        for (size_t j = i + 1; j < dims.size(); ++j) stride *= dims[j];
        if (stride != 1) out << "  imulq $" << stride << ", %rax\n";
        if (first) {
            out << "  pushq %rax\n";
            first = false;
        } else {
            out << "  popq %rcx\n";
            out << "  addq %rcx, %rax\n";
            out << "  pushq %rax\n";
        }
    }
    out << "  popq %rax\n";
}

Type CodeGenVisitor::typeAfterArrayAccess(const ArrayAccessExpr* access) const {
    auto it = localTypes.find(access->name);
    if (it == localTypes.end()) return Type::Error();
    Type result = it->second;
    if (access->indices.size() > result.dimensions.size()) return Type::Error();
    result.dimensions.erase(result.dimensions.begin(), result.dimensions.begin() + static_cast<long>(access->indices.size()));
    return result;
}

bool CodeGenVisitor::isCharExpr(Expr* expr) const {
    if (dynamic_cast<CharExpr*>(expr)) return true;
    if (auto* id = dynamic_cast<IdExpr*>(expr)) {
        auto it = localTypes.find(id->name);
        return it != localTypes.end() && it->second.kind == TypeKind::Char && it->second.pointerDepth == 0 && !it->second.isArray();
    }
    if (auto* access = dynamic_cast<ArrayAccessExpr*>(expr)) {
        Type t = typeAfterArrayAccess(access);
        return t.kind == TypeKind::Char && t.pointerDepth == 0 && !t.isArray();
    }
    if (auto* field = dynamic_cast<FieldAccessExpr*>(expr)) {
        auto it = localTypes.find(field->fullName());
        return it != localTypes.end() && it->second.kind == TypeKind::Char && it->second.pointerDepth == 0 && !it->second.isArray();
    }
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        Type ret = returnTypeForCall(call);
        return ret.kind == TypeKind::Char && ret.pointerDepth == 0 && !ret.isArray();
    }
    return false;
}

bool CodeGenVisitor::isStringExpr(Expr* expr) const {
    if (dynamic_cast<StringExpr*>(expr)) return true;
    if (auto* id = dynamic_cast<IdExpr*>(expr)) {
        auto it = localTypes.find(id->name);
        return it != localTypes.end() && it->second.kind == TypeKind::String && it->second.pointerDepth == 0 && !it->second.isArray();
    }
    if (auto* access = dynamic_cast<ArrayAccessExpr*>(expr)) {
        Type t = typeAfterArrayAccess(access);
        return t.kind == TypeKind::String && t.pointerDepth == 0 && !t.isArray();
    }
    if (auto* field = dynamic_cast<FieldAccessExpr*>(expr)) {
        auto it = localTypes.find(field->fullName());
        return it != localTypes.end() && it->second.kind == TypeKind::String && it->second.pointerDepth == 0 && !it->second.isArray();
    }
    if (auto* call = dynamic_cast<CallExpr*>(expr)) {
        Type ret = returnTypeForCall(call);
        return ret.kind == TypeKind::String && ret.pointerDepth == 0 && !ret.isArray();
    }
    return false;
}

std::string CodeGenVisitor::emitStringConstant(const std::string& value) {
    auto found = stringLabels.find(value);
    if (found != stringLabels.end()) return found->second;

    std::string label = newLabel(".Lstr");
    stringLabels[value] = label;
    out << "  .pushsection .rodata\n";
    out << label << ": .string \"" << escapeString(value) << "\"\n";
    out << "  .popsection\n";
    return label;
}

std::string CodeGenVisitor::escapeString(const std::string& value) {
    std::ostringstream outEsc;
    for (char c : value) {
        if (c == '"') outEsc << "\\\"";
        else if (c == '\\') outEsc << "\\\\";
        else if (c == '\n') outEsc << "\\n";
        else outEsc << c;
    }
    return outEsc.str();
}
