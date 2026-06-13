#include "typecheck_visitor.h"
#include <iostream>

void TypeCheckVisitor::check(Program& p) {
    p.accept(*this);
}

void TypeCheckVisitor::visit(Program* node) {
    program = node;
    functions.clear();
    structs.clear();
    for (const auto& st : node->structs) {
        if (structs.count(st->name)) {
            throw SemanticError(st->loc, "struct redeclarado: '" + st->name + "'");
        }
        structs[st->name] = st.get();
    }
    for (const auto& st : node->structs) st->accept(*this);
    for (const auto& fn : node->functions) {
        if (functions.count(fn->name)) {
            throw SemanticError(fn->loc, "función redeclarada: '" + fn->name + "'");
        }
        if (fn->isTemplate() && fn->templateParam != "T") {
            throw SemanticError(fn->loc, "esta versión solo soporta templates con un parámetro llamado T");
        }
        functions[fn->name] = fn.get();
    }
    if (!functions.count("main")) {
        throw SemanticError(node->loc, "el programa debe declarar una función main");
    }
    for (auto& fn : node->functions) fn->accept(*this);
}

void TypeCheckVisitor::visit(StructDecl* node) {
    std::unordered_map<std::string, bool> seen;
    for (const auto& field : node->fields) {
        if (seen[field.name]) throw SemanticError(field.loc, "campo redeclarado en struct '" + node->name + "': '" + field.name + "'");
        seen[field.name] = true;
        validateKnownType(field.type, field.loc);
    }
}

void TypeCheckVisitor::visit(FunctionDecl* node) {
    validateKnownType(node->returnType, node->loc);
    symbols.enterScope();
    currentFunctionReturn = node->returnType;
    for (const auto& param : node->params) {
        validateKnownType(param.type, param.loc);
        if (!symbols.declare(param.name, param.type, param.loc, true)) {
            throw SemanticError(param.loc, "parámetro redeclarado: '" + param.name + "'");
        }
    }
    node->body->accept(*this);
    symbols.exitScope();
}

void TypeCheckVisitor::visit(BlockStmt* node) {
    symbols.enterScope();
    for (auto& stmt : node->statements) stmt->accept(*this);
    symbols.exitScope();
}

void TypeCheckVisitor::visit(VarDeclStmt* node) {
    Type declared = node->type;
    if (!node->inferred) validateKnownType(declared, node->loc);
    if (node->initializer) {
        Type initType = infer(node->initializer.get());
        if (node->inferred) declared = initType;
        else if (!declared.isAssignableFrom(initType)) {
            throw SemanticError(node->loc, "no se puede inicializar variable '" + node->name + "' de tipo " + declared.toString() + " con valor " + initType.toString());
        }
    }
    if (declared.kind == TypeKind::Void && declared.pointerDepth == 0) {
        throw SemanticError(node->loc, "no se puede declarar variable '" + node->name + "' de tipo void");
    }
    if (declared.kind == TypeKind::Unknown) {
        throw SemanticError(node->loc, "no se pudo inferir el tipo de '" + node->name + "'");
    }
    node->type = declared;
    if (!symbols.declare(node->name, declared, node->loc)) {
        throw SemanticError(node->loc, "variable redeclarada en el mismo scope: '" + node->name + "'");
    }
}

void TypeCheckVisitor::visit(AssignStmt* node) {
    Symbol* symbol = symbols.resolve(node->name);
    if (!symbol) throw SemanticError(node->loc, "variable no declarada: '" + node->name + "'");
    if (symbol->type.isArray()) {
        throw SemanticError(node->loc, "no se puede asignar directamente al arreglo '" + node->name + "'. Usa " + node->name + "[índice] = valor");
    }
    Type valueType = infer(node->value.get());
    if (!symbol->type.isAssignableFrom(valueType)) {
        throw SemanticError(node->loc, "asignación inválida: '" + node->name + "' es " + symbol->type.toString() + " pero recibe " + valueType.toString());
    }
}

void TypeCheckVisitor::visit(ArrayAssignStmt* node) {
    std::vector<Expr*> rawIndices;
    for (auto& idx : node->indices) rawIndices.push_back(idx.get());
    Type elemType = arrayElementType(node->name, rawIndices, node->loc);
    if (elemType.isArray()) {
        throw SemanticError(node->loc, "la asignación a arreglo requiere especificar todos los índices de '" + node->name + "'. Falta(n) dimensión(es)");
    }
    Type valueType = infer(node->value.get());
    if (!elemType.isAssignableFrom(valueType)) {
        throw SemanticError(node->loc, "asignación inválida: elemento de '" + node->name + "' es " + elemType.toString() + " pero recibe " + valueType.toString());
    }
}

void TypeCheckVisitor::visit(PointerAssignStmt* node) {
    Symbol* symbol = symbols.resolve(node->pointerName);
    if (!symbol) throw SemanticError(node->loc, "puntero no declarado: '" + node->pointerName + "'");
    if (symbol->type.pointerDepth <= 0 || symbol->type.isArray()) {
        throw SemanticError(node->loc, "'*' solo puede asignar mediante punteros, pero '" + node->pointerName + "' es " + symbol->type.toString());
    }
    Type targetType = symbol->type;
    targetType.pointerDepth--;
    Type valueType = infer(node->value.get());
    if (!targetType.isAssignableFrom(valueType)) {
        throw SemanticError(node->loc, "asignación inválida por puntero: '*" + node->pointerName + "' es " + targetType.toString() + " pero recibe " + valueType.toString());
    }
}

void TypeCheckVisitor::visit(FieldAssignStmt* node) {
    Type fType = fieldType(node->objectName, node->fieldName, node->loc);
    Type valueType = infer(node->value.get());
    if (!fType.isAssignableFrom(valueType)) {
        throw SemanticError(node->loc, "asignación inválida: campo '" + node->fullName() + "' es " + fType.toString() + " pero recibe " + valueType.toString());
    }
}

void TypeCheckVisitor::visit(DeleteStmt* node) {
    Symbol* symbol = symbols.resolve(node->pointerName);
    if (!symbol) throw SemanticError(node->loc, "puntero no declarado en delete: '" + node->pointerName + "'");
    if (symbol->type.pointerDepth <= 0 || symbol->type.isArray()) {
        throw SemanticError(node->loc, "delete requiere un puntero, pero '" + node->pointerName + "' es " + symbol->type.toString());
    }
}

void TypeCheckVisitor::visit(PrintStmt* node) {
    infer(node->expression.get());
}

void TypeCheckVisitor::visit(ReturnStmt* node) {
    Type valueType = Type::Void();
    if (node->value) valueType = infer(node->value.get());
    if (!currentFunctionReturn.isAssignableFrom(valueType)) {
        throw SemanticError(node->loc, "retorno inválido. Esperado: " + currentFunctionReturn.toString() + ", recibido: " + valueType.toString());
    }
}

void TypeCheckVisitor::visit(IfStmt* node) {
    requireCondition(infer(node->condition.get()), node->condition->loc, "if");
    node->thenBranch->accept(*this);
    if (node->elseBranch) node->elseBranch->accept(*this);
}

void TypeCheckVisitor::visit(WhileStmt* node) {
    requireCondition(infer(node->condition.get()), node->condition->loc, "while");
    node->body->accept(*this);
}

void TypeCheckVisitor::visit(TraceStmt* node) {
    for (const auto& name : node->names) {
        Type targetType = Type::Unknown();
        auto dot = name.find('.');
        if (dot != std::string::npos) {
            targetType = fieldType(name.substr(0, dot), name.substr(dot + 1), node->loc);
        } else {
            Symbol* symbol = symbols.resolve(name);
            if (!symbol) {
                throw SemanticError(node->loc, "trace " + traceKindName(node->kind) + " usa variable no declarada: '" + name + "'");
            }
            targetType = symbol->type;
        }
        if (node->kind == TraceKind::Array && !targetType.isArray()) {
            throw SemanticError(node->loc, "trace array requiere un arreglo, pero '" + name + "' es " + targetType.toString());
        }
    }
}

void TypeCheckVisitor::visit(ExprStmt* node) {
    infer(node->expression.get());
}

Type TypeCheckVisitor::infer(Expr* expr) {
    expr->accept(*this);
    return lastType;
}

void TypeCheckVisitor::visit(BinaryExpr* node) {
    Type left = infer(node->left.get());
    Type right = infer(node->right.get());
    const std::string& op = node->op;

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        if (!left.isNumeric() || !right.isNumeric()) {
            throw SemanticError(node->loc, "operador '" + op + "' requiere operandos numéricos");
        }
        lastType = Type::Int();
        return;
    }

    if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        if (!left.isNumeric() || !right.isNumeric()) {
            throw SemanticError(node->loc, "comparación '" + op + "' requiere operandos numéricos");
        }
        lastType = Type::Bool();
        return;
    }

    if (op == "==" || op == "!=") {
        if (!left.isAssignableFrom(right) && !right.isAssignableFrom(left)) {
            throw SemanticError(node->loc, "comparación entre tipos incompatibles: " + left.toString() + " y " + right.toString());
        }
        lastType = Type::Bool();
        return;
    }

    if (op == "&&" || op == "||") {
        if (!left.isBoolean() || !right.isBoolean()) {
            throw SemanticError(node->loc, "operador lógico '" + op + "' requiere bool y bool");
        }
        lastType = Type::Bool();
        return;
    }

    throw SemanticError(node->loc, "operador desconocido: " + op);
}

void TypeCheckVisitor::visit(UnaryExpr* node) {
    Type right = infer(node->right.get());
    if (node->op == "-") {
        if (!right.isNumeric()) throw SemanticError(node->loc, "operador '-' requiere tipo numérico");
        lastType = Type::Int();
        return;
    }
    if (node->op == "!") {
        if (!right.isBoolean()) throw SemanticError(node->loc, "operador '!' requiere bool");
        lastType = Type::Bool();
        return;
    }
    if (node->op == "&") {
        if (!dynamic_cast<IdExpr*>(node->right.get())) {
            throw SemanticError(node->loc, "operador '&' solo soporta variables simples en esta versión");
        }
        if (right.kind == TypeKind::Void || right.isArray()) {
            throw SemanticError(node->loc, "no se puede obtener dirección de tipo " + right.toString());
        }
        right.pointerDepth++;
        lastType = right;
        return;
    }
    if (node->op == "*") {
        if (right.pointerDepth <= 0) {
            throw SemanticError(node->loc, "operador '*' requiere un puntero, pero recibe " + right.toString());
        }
        right.pointerDepth--;
        lastType = right;
        return;
    }
    throw SemanticError(node->loc, "operador unario desconocido: " + node->op);
}

void TypeCheckVisitor::visit(IntExpr*) { lastType = Type::Int(); }
void TypeCheckVisitor::visit(BoolExpr*) { lastType = Type::Bool(); }
void TypeCheckVisitor::visit(CharExpr*) { lastType = Type::Char(); }
void TypeCheckVisitor::visit(StringExpr*) { lastType = Type::String(); }

void TypeCheckVisitor::visit(IdExpr* node) {
    Symbol* symbol = symbols.resolve(node->name);
    if (!symbol) throw SemanticError(node->loc, "variable no declarada: '" + node->name + "'");
    lastType = symbol->type;
}

void TypeCheckVisitor::visit(ArrayAccessExpr* node) {
    std::vector<Expr*> rawIndices;
    for (auto& idx : node->indices) rawIndices.push_back(idx.get());
    lastType = arrayElementType(node->name, rawIndices, node->loc);
}

void TypeCheckVisitor::visit(FieldAccessExpr* node) {
    lastType = fieldType(node->objectName, node->fieldName, node->loc);
}

void TypeCheckVisitor::visit(NewExpr* node) {
    if (node->allocatedType.kind != TypeKind::Int || node->allocatedType.pointerDepth != 0 || node->allocatedType.isArray()) {
        throw SemanticError(node->loc, "esta versión solo soporta new int");
    }
    Type result = node->allocatedType;
    result.pointerDepth++;
    lastType = result;
}

void TypeCheckVisitor::visit(LambdaExpr* node) {
    validateKnownType(node->returnType, node->loc);

    SymbolTable savedSymbols = symbols;
    Type savedReturn = currentFunctionReturn;

    // Las lambdas de esta versión son sin captura: se verifican en un scope aislado.
    symbols = SymbolTable();
    currentFunctionReturn = node->returnType;

    std::unordered_map<std::string, bool> seenParams;
    std::vector<Type> paramTypes;
    for (const auto& param : node->params) {
        validateKnownType(param.type, param.loc);
        if (seenParams[param.name]) throw SemanticError(param.loc, "parámetro redeclarado en lambda: '" + param.name + "'");
        seenParams[param.name] = true;
        paramTypes.push_back(param.type);
        if (!symbols.declare(param.name, param.type, param.loc, true)) {
            throw SemanticError(param.loc, "parámetro redeclarado en lambda: '" + param.name + "'");
        }
    }

    node->body->accept(*this);

    symbols = savedSymbols;
    currentFunctionReturn = savedReturn;
    lastType = Type::Lambda(std::move(paramTypes), node->returnType);
}

void TypeCheckVisitor::visit(CallExpr* node) {
    auto it = functions.find(node->callee);
    if (it != functions.end()) {
        FunctionDecl* fn = it->second;

        Type concreteType = Type::Unknown();
        if (fn->isTemplate()) {
            if (node->typeArguments.size() != 1) {
                throw SemanticError(node->loc, "la función template '" + node->callee + "' requiere exactamente un argumento de tipo, por ejemplo " + node->callee + "<int>(...)");
            }
            concreteType = node->typeArguments[0];
            validateKnownType(concreteType, node->loc);
            if (containsGeneric(concreteType)) {
                throw SemanticError(node->loc, "no se puede especializar template con tipo genérico: " + concreteType.toString());
            }
        } else if (!node->typeArguments.empty()) {
            throw SemanticError(node->loc, "la función '" + node->callee + "' no es template y no acepta argumentos de tipo");
        }

        if (fn->params.size() != node->arguments.size()) {
            throw SemanticError(node->loc, "la función '" + node->callee + "' espera " + std::to_string(fn->params.size()) + " argumentos, pero recibió " + std::to_string(node->arguments.size()));
        }
        for (size_t i = 0; i < node->arguments.size(); ++i) {
            Type expected = fn->isTemplate() ? specializeType(fn->params[i].type, fn->templateParam, concreteType) : fn->params[i].type;
            Type argType = infer(node->arguments[i].get());
            if (!expected.isAssignableFrom(argType)) {
                throw SemanticError(node->arguments[i]->loc, "argumento " + std::to_string(i + 1) + " inválido en llamada a '" + node->callee + "'. Esperado: " + expected.toString() + ", recibido: " + argType.toString());
            }
        }
        lastType = fn->isTemplate() ? specializeType(fn->returnType, fn->templateParam, concreteType) : fn->returnType;
        return;
    }

    Symbol* symbol = symbols.resolve(node->callee);
    if (!symbol || symbol->type.kind != TypeKind::Lambda) {
        throw SemanticError(node->loc, "función o lambda no declarada: '" + node->callee + "'");
    }

    Type lambdaType = symbol->type;
    if (lambdaType.lambdaParams.size() != node->arguments.size()) {
        throw SemanticError(node->loc, "la lambda '" + node->callee + "' espera " + std::to_string(lambdaType.lambdaParams.size()) + " argumentos, pero recibió " + std::to_string(node->arguments.size()));
    }
    for (size_t i = 0; i < node->arguments.size(); ++i) {
        Type argType = infer(node->arguments[i].get());
        if (!lambdaType.lambdaParams[i].isAssignableFrom(argType)) {
            throw SemanticError(node->arguments[i]->loc, "argumento " + std::to_string(i + 1) + " inválido en llamada a lambda '" + node->callee + "'. Esperado: " + lambdaType.lambdaParams[i].toString() + ", recibido: " + argType.toString());
        }
    }
    lastType = lambdaType.lambdaReturn ? *lambdaType.lambdaReturn : Type::Unknown();
}

Type TypeCheckVisitor::arrayElementType(const std::string& name, const std::vector<Expr*>& indices, const SourceLocation& loc) {
    Symbol* symbol = symbols.resolve(name);
    if (!symbol) throw SemanticError(loc, "arreglo no declarado: '" + name + "'");
    if (!symbol->type.isArray()) throw SemanticError(loc, "'" + name + "' no es un arreglo, es " + symbol->type.toString());
    if (indices.empty()) throw SemanticError(loc, "acceso a arreglo sin índices: '" + name + "'");
    if (indices.size() > symbol->type.dimensions.size()) {
        throw SemanticError(loc, "demasiados índices para '" + name + "'. Esperado: " + std::to_string(symbol->type.dimensions.size()) + ", recibido: " + std::to_string(indices.size()));
    }

    for (size_t i = 0; i < indices.size(); ++i) {
        Expr* index = indices[i];
        Type indexType = infer(index);
        if (indexType.kind != TypeKind::Int || indexType.pointerDepth != 0 || indexType.isArray()) {
            throw SemanticError(index->loc, "el índice de un arreglo debe ser int, pero es " + indexType.toString());
        }

        if (auto* constant = dynamic_cast<IntExpr*>(index)) {
            int size = symbol->type.dimensions[i];
            if (constant->value < 0 || constant->value >= size) {
                throw SemanticError(index->loc, "índice fuera de rango en '" + name + "' dimensión " + std::to_string(i + 1) + ": " + std::to_string(constant->value) + " no está entre 0 y " + std::to_string(size - 1));
            }
        }
    }

    Type result = symbol->type;
    result.dimensions.erase(result.dimensions.begin(), result.dimensions.begin() + static_cast<long>(indices.size()));
    return result;
}

void TypeCheckVisitor::requireCondition(const Type& type, const SourceLocation& loc, const std::string& constructName) {
    if (!type.isBoolean()) {
        throw SemanticError(loc, "la condición de '" + constructName + "' debe ser bool, pero es " + type.toString());
    }
}


void TypeCheckVisitor::validateKnownType(const Type& type, const SourceLocation& loc) {
    if (type.kind == TypeKind::Generic) {
        if (type.genericName != "T") throw SemanticError(loc, "parámetro genérico no soportado: '" + type.genericName + "'");
        return;
    }
    if (type.kind == TypeKind::Struct && !structs.count(type.structName)) {
        throw SemanticError(loc, "tipo struct no declarado: '" + type.structName + "'");
    }
    if (type.kind == TypeKind::Lambda) {
        for (const auto& paramType : type.lambdaParams) validateKnownType(paramType, loc);
        if (type.lambdaReturn) validateKnownType(*type.lambdaReturn, loc);
    }
}


Type TypeCheckVisitor::specializeType(const Type& type, const std::string& genericName, const Type& concrete) const {
    if (type.kind == TypeKind::Generic && type.genericName == genericName && type.pointerDepth == 0 && !type.isArray()) {
        return concrete;
    }
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

bool TypeCheckVisitor::containsGeneric(const Type& type) const {
    if (type.kind == TypeKind::Generic) return true;
    if (type.kind == TypeKind::Lambda) {
        for (const auto& param : type.lambdaParams) if (containsGeneric(param)) return true;
        return type.lambdaReturn && containsGeneric(*type.lambdaReturn);
    }
    return false;
}

Type TypeCheckVisitor::fieldType(const std::string& objectName, const std::string& fieldName, const SourceLocation& loc) {
    Symbol* symbol = symbols.resolve(objectName);
    if (!symbol) throw SemanticError(loc, "variable no declarada: '" + objectName + "'");
    if (symbol->type.kind != TypeKind::Struct || symbol->type.pointerDepth != 0 || symbol->type.isArray()) {
        throw SemanticError(loc, "'" + objectName + "' no es un struct, es " + symbol->type.toString());
    }
    auto it = structs.find(symbol->type.structName);
    if (it == structs.end()) throw SemanticError(loc, "struct no declarado: '" + symbol->type.structName + "'");
    const Param* field = it->second->findField(fieldName);
    if (!field) throw SemanticError(loc, "el struct '" + symbol->type.structName + "' no tiene campo '" + fieldName + "'");
    return field->type;
}
