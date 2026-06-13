#include "trace_visitor.h"
#include "errors.h"
#include <algorithm>

RuntimeValue RuntimeValue::Int(long v) { RuntimeValue r; r.kind = TypeKind::Int; r.intValue = v; return r; }
RuntimeValue RuntimeValue::Bool(bool v) { RuntimeValue r; r.kind = TypeKind::Bool; r.boolValue = v; return r; }
RuntimeValue RuntimeValue::Char(char v) { RuntimeValue r; r.kind = TypeKind::Char; r.intValue = static_cast<unsigned char>(v); return r; }
RuntimeValue RuntimeValue::String(std::string v) { RuntimeValue r; r.kind = TypeKind::String; r.stringValue = std::move(v); return r; }
RuntimeValue RuntimeValue::Pointer(std::string target) { RuntimeValue r; r.isPointer = true; r.kind = TypeKind::Int; r.pointsTo = std::move(target); return r; }
RuntimeValue RuntimeValue::Lambda(LambdaExpr* lambdaExpr, std::string name) { RuntimeValue r; r.isLambda = true; r.kind = TypeKind::Lambda; r.lambda = lambdaExpr; r.lambdaName = std::move(name); return r; }
RuntimeValue RuntimeValue::Array(TypeKind elementKind, const std::vector<int>& dimensions) {
    RuntimeValue r;
    r.kind = elementKind;
    r.isArray = true;
    r.dimensions = dimensions;
    RuntimeValue zero;
    if (elementKind == TypeKind::Bool) zero = RuntimeValue::Bool(false);
    else if (elementKind == TypeKind::Char) zero = RuntimeValue::Char('\0');
    else if (elementKind == TypeKind::String) zero = RuntimeValue::String("");
    else zero = RuntimeValue::Int(0);
    long total = 1;
    for (int dim : dimensions) total *= dim;
    r.elements.assign(static_cast<size_t>(total), zero);
    return r;
}

RuntimeValue RuntimeValue::Struct(std::string name) {
    RuntimeValue r;
    r.kind = TypeKind::Struct;
    r.isStruct = true;
    r.structName = std::move(name);
    return r;
}

std::string RuntimeValue::toString() const {
    if (isLambda) {
        return "<lambda" + (lambdaName.empty() ? std::string("") : std::string(" ") + lambdaName) + ">";
    }
    if (isPointer) {
        if (pointsTo.empty()) return "null";
        return "&" + pointsTo;
    }
    if (isArray) {
        std::string text = "[";
        for (size_t i = 0; i < elements.size(); ++i) {
            if (i > 0) text += ", ";
            text += elements[i].toString();
        }
        text += "]";
        return text;
    }
    if (isStruct) {
        std::string text = "{";
        bool first = true;
        for (const auto& entry : fields) {
            if (!first) text += ", ";
            first = false;
            text += entry.first + ": " + entry.second.toString();
        }
        text += "}";
        return text;
    }
    switch (kind) {
        case TypeKind::Int: return std::to_string(intValue);
        case TypeKind::Bool: return boolValue ? "true" : "false";
        case TypeKind::Char: return std::string("'") + static_cast<char>(intValue) + "'";
        case TypeKind::String: return stringValue;
        default: return "<indefinido>";
    }
}

bool RuntimeValue::truthy() const {
    if (kind == TypeKind::Bool) return boolValue;
    if (kind == TypeKind::Int || kind == TypeKind::Char) return intValue != 0;
    return false;
}

TraceVisitor::TraceVisitor(std::ostream& o) : out(o) {}

void TraceVisitor::run(Program& p) {
    p.accept(*this);
}

void TraceVisitor::visit(Program* node) {
    program = node;
    structDefs.clear();
    heapMemory.clear();
    nextHeapId = 1;
    for (auto& st : node->structs) structDefs[st->name] = st.get();
    FunctionDecl* mainFn = node->findFunction("main");
    if (!mainFn) throw SemanticError(node->loc, "no se encontró función main");
    out << "=== EduTrace: traza educativa ===\n";
    callFunction(mainFn, {});
    out << "=== Fin de traza ===\n";
}

RuntimeValue TraceVisitor::callFunction(FunctionDecl* fn, const std::vector<RuntimeValue>& args) {
    callStack.push_back(fn->name);
    envStack.emplace_back();

    if (traceStack || traceRecursion) {
        out << indent() << "Entrando a función: " << fn->name << "\n";
        printCallStack("pila de llamadas");
    }

    for (size_t i = 0; i < fn->params.size(); ++i) {
        declareVar(fn->params[i].name, args[i]);
        if (traceStack) out << indent() << "parámetro " << fn->params[i].name << " = " << args[i].toString() << "\n";
    }

    if (traceStack && !fn->params.empty()) {
        printCurrentStackFrame("frame actual después de recibir parámetros");
    }

    bool prevReturning = returning;
    RuntimeValue prevReturn = returnValue;
    returning = false;
    fn->body->accept(*this);
    RuntimeValue result = returning ? returnValue : RuntimeValue::Int(0);

    if (traceStack || traceRecursion) {
        out << indent() << "Retorna " << result.toString() << " desde " << fn->name << "\n";
    }

    returning = prevReturning;
    returnValue = prevReturn;
    envStack.pop_back();
    callStack.pop_back();
    return result;
}

RuntimeValue TraceVisitor::callLambda(LambdaExpr* lambda, const std::string& name, const std::vector<RuntimeValue>& args) {
    std::string displayName = name.empty() ? "lambda" : "lambda " + name;
    callStack.push_back(displayName);
    envStack.emplace_back();

    if (traceStack || traceRecursion) {
        out << indent() << "Entrando a lambda: " << (name.empty() ? "<anonima>" : name) << "\n";
        printCallStack("pila de llamadas");
    }

    for (size_t i = 0; i < lambda->params.size(); ++i) {
        declareVar(lambda->params[i].name, args[i]);
        if (traceStack) out << indent() << "parámetro " << lambda->params[i].name << " = " << args[i].toString() << "\n";
    }

    bool prevReturning = returning;
    RuntimeValue prevReturn = returnValue;
    returning = false;
    lambda->body->accept(*this);
    RuntimeValue result = returning ? returnValue : RuntimeValue::Int(0);

    if (traceStack || traceRecursion) {
        out << indent() << "Retorna " << result.toString() << " desde " << displayName << "\n";
    }

    returning = prevReturning;
    returnValue = prevReturn;
    envStack.pop_back();
    callStack.pop_back();
    return result;
}

void TraceVisitor::visit(StructDecl*) {}
void TraceVisitor::visit(FunctionDecl*) {}

void TraceVisitor::visit(BlockStmt* node) {
    envStack.emplace_back();
    for (auto& stmt : node->statements) {
        if (returning) break;
        stmt->accept(*this);
    }
    envStack.pop_back();
}

void TraceVisitor::visit(VarDeclStmt* node) {
    RuntimeValue value;
    if (node->initializer) value = eval(node->initializer.get());
    else value = defaultValueForType(node->type);
    if (value.isLambda) value.lambdaName = node->name;

    if (node->inferred) {
        out << indent() << "Inferencia de tipo: " << node->name
            << " = " << (node->initializer ? exprToString(node->initializer.get()) : "<sin inicializador>")
            << " => " << node->type.toString() << "\n";
    }

    declareVar(node->name, value);
    if (watchedVariables.count(node->name)) out << indent() << "Declaración: " << node->name << " = " << value.toString() << "\n";
}

void TraceVisitor::visit(AssignStmt* node) {
    RuntimeValue value = eval(node->value.get());
    RuntimeValue oldValue = getVar(node->name, node->loc);
    setVar(node->name, value, node->loc);
    if (watchedVariables.count(node->name)) {
        out << indent() << "Variable " << node->name << " cambió: " << oldValue.toString() << " -> " << value.toString() << "\n";
    }
    if (watchedMemoryNames.count(node->name)) {
        out << indent() << "Memoria: " << node->name << " cambió: " << oldValue.toString() << " -> " << value.toString() << "\n";
        printWatchedMemoryState("estado de memoria actualizado");
    }
}

void TraceVisitor::visit(ArrayAssignStmt* node) {
    std::vector<long> indices;
    for (auto& idxExpr : node->indices) {
        RuntimeValue idx = eval(idxExpr.get());
        indices.push_back(idx.intValue);
    }
    RuntimeValue value = eval(node->value.get());
    RuntimeValue oldValue = getArrayElement(node->name, indices, node->loc);
    setArrayElement(node->name, indices, value, node->loc);
    if (watchedArrays.count(node->name)) {
        out << indent() << "Arreglo " << node->name;
        for (long idx : indices) out << "[" << idx << "]";
        out << " cambió: " << oldValue.toString() << " -> " << value.toString() << "\n";
        printArray(node->name, "estado actual de " + node->name);
    }
}

void TraceVisitor::visit(PointerAssignStmt* node) {
    RuntimeValue ptr = getVar(node->pointerName, node->loc);
    if (!ptr.isPointer || ptr.pointsTo.empty()) {
        throw SemanticError(node->loc, "puntero no inicializado durante trace: " + node->pointerName);
    }
    RuntimeValue value = eval(node->value.get());
    RuntimeValue oldValue = getPathValue(ptr.pointsTo, node->loc);
    setPathValue(ptr.pointsTo, value, node->loc);
    if (!watchedMemoryNames.empty()) {
        out << indent() << "Ejecutando *" << node->pointerName << " = " << value.toString() << "\n";
        out << indent() << "  " << ptr.pointsTo << " cambió: " << oldValue.toString() << " -> " << value.toString() << "\n";
        printWatchedMemoryState("estado de memoria actualizado");
    }
}

void TraceVisitor::visit(FieldAssignStmt* node) {
    RuntimeValue value = eval(node->value.get());
    RuntimeValue oldValue = getFieldValue(node->objectName, node->fieldName, node->loc);
    setFieldValue(node->objectName, node->fieldName, value, node->loc);
    if (watchedVariables.count(node->fullName())) {
        out << indent() << "Variable " << node->fullName() << " cambió: " << oldValue.toString() << " -> " << value.toString() << "\n";
    }
}

void TraceVisitor::visit(DeleteStmt* node) {
    RuntimeValue ptr = getVar(node->pointerName, node->loc);
    if (!ptr.isPointer || ptr.pointsTo.empty()) {
        throw SemanticError(node->loc, "delete sobre puntero nulo o no inicializado durante trace: " + node->pointerName);
    }
    out << indent() << "Liberando memoria dinámica apuntada por " << node->pointerName << "\n";
    if (ptr.pointsTo.rfind("heap#", 0) == 0) {
        heapMemory.erase(ptr.pointsTo);
        out << indent() << "  bloque " << ptr.pointsTo << " liberado\n";
    } else {
        out << indent() << "  advertencia: " << node->pointerName << " apunta a variable local " << ptr.pointsTo << ", no a memoria dinámica\n";
    }
    setVar(node->pointerName, RuntimeValue::Pointer(""), node->loc);
    if (!watchedMemoryNames.empty()) printWatchedMemoryState("estado de memoria después de delete");
}

void TraceVisitor::visit(PrintStmt* node) {
    RuntimeValue value = eval(node->expression.get());
    out << indent() << "print: " << value.toString() << "\n";
}

void TraceVisitor::visit(ReturnStmt* node) {
    returnValue = node->value ? eval(node->value.get()) : RuntimeValue::Int(0);
    returning = true;
}

void TraceVisitor::visit(IfStmt* node) {
    RuntimeValue cond = eval(node->condition.get());
    if (traceBranch) {
        out << indent() << "Evaluando condición if:\n";
        out << indent() << "  " << exprToString(node->condition.get()) << " => " << cond.toString() << "\n";
        out << indent() << "Se ejecuta la rama " << (cond.truthy() ? "THEN" : "ELSE") << ".\n";
    }
    if (cond.truthy()) node->thenBranch->accept(*this);
    else if (node->elseBranch) node->elseBranch->accept(*this);
}

void TraceVisitor::visit(WhileStmt* node) {
    int iter = 1;
    while (!returning) {
        RuntimeValue cond = eval(node->condition.get());
        bool shouldTraceLoop = traceLoop || !watchedLoopVariables.empty();

        if (shouldTraceLoop) {
            out << indent() << "Evaluando condición while:\n";
            out << indent() << "  " << exprToString(node->condition.get()) << " => " << cond.toString() << "\n";
        }

        if (!cond.truthy()) {
            if (shouldTraceLoop) out << indent() << "Fin del ciclo: condición = false.\n";
            break;
        }

        if (shouldTraceLoop) {
            out << indent() << "Iteración " << iter << " del while\n";
            if (!watchedLoopVariables.empty()) printWatchedVariables(watchedLoopVariables, "estado antes");
        }

        node->body->accept(*this);

        if (shouldTraceLoop && !watchedLoopVariables.empty()) printWatchedVariables(watchedLoopVariables, "estado después");
        iter++;
        if (iter > 100000) throw CompilerError("posible ciclo infinito durante trace");
    }
}

void TraceVisitor::visit(TraceStmt* node) {
    switch (node->kind) {
        case TraceKind::Variables:
            for (auto& n : node->names) watchedVariables.insert(n);
            printWatchedVariables(watchedVariables, "trace variables activado");
            break;
        case TraceKind::Loop:
            traceLoop = true;
            for (auto& n : node->names) watchedLoopVariables.insert(n);
            out << indent() << "trace loop activado";
            if (!node->names.empty()) out << " para variables observadas";
            out << "\n";
            break;
        case TraceKind::Branch:
            traceBranch = true;
            out << indent() << "trace branch activado\n";
            break;
        case TraceKind::Stack:
            traceStack = true;
            printCurrentStackFrame("trace stack activado");
            break;
        case TraceKind::Recursion:
            traceRecursion = true;
            out << indent() << "trace recursion activado\n";
            break;
        case TraceKind::Array:
            for (auto& n : node->names) {
                watchedArrays.insert(n);
                printArray(n, "trace array activado");
            }
            if (node->names.empty()) out << indent() << "trace array activado sin arreglos específicos\n";
            break;
        case TraceKind::Memory:
            for (auto& n : node->names) watchedMemoryNames.insert(n);
            printMemoryState(node->names, "trace memory activado");
            break;
    }
}

void TraceVisitor::visit(ExprStmt* node) { eval(node->expression.get()); }

RuntimeValue TraceVisitor::eval(Expr* expr) {
    expr->accept(*this);
    return lastValue;
}

void TraceVisitor::visit(BinaryExpr* node) {
    RuntimeValue l = eval(node->left.get());
    RuntimeValue r = eval(node->right.get());
    const std::string& op = node->op;
    if (op == "+") lastValue = RuntimeValue::Int(l.intValue + r.intValue);
    else if (op == "-") lastValue = RuntimeValue::Int(l.intValue - r.intValue);
    else if (op == "*") lastValue = RuntimeValue::Int(l.intValue * r.intValue);
    else if (op == "/") lastValue = RuntimeValue::Int(r.intValue == 0 ? 0 : l.intValue / r.intValue);
    else if (op == "%") lastValue = RuntimeValue::Int(r.intValue == 0 ? 0 : l.intValue % r.intValue);
    else if (op == "<") lastValue = RuntimeValue::Bool(l.intValue < r.intValue);
    else if (op == "<=") lastValue = RuntimeValue::Bool(l.intValue <= r.intValue);
    else if (op == ">") lastValue = RuntimeValue::Bool(l.intValue > r.intValue);
    else if (op == ">=") lastValue = RuntimeValue::Bool(l.intValue >= r.intValue);
    else if (op == "==") lastValue = RuntimeValue::Bool(l.toString() == r.toString());
    else if (op == "!=") lastValue = RuntimeValue::Bool(l.toString() != r.toString());
    else if (op == "&&") lastValue = RuntimeValue::Bool(l.truthy() && r.truthy());
    else if (op == "||") lastValue = RuntimeValue::Bool(l.truthy() || r.truthy());
}

void TraceVisitor::visit(UnaryExpr* node) {
    if (node->op == "&") {
        if (auto* id = dynamic_cast<IdExpr*>(node->right.get())) {
            // Evalúa primero para asegurar que la variable exista.
            getVar(id->name, node->loc);
            lastValue = RuntimeValue::Pointer(id->name);
            return;
        }
        throw SemanticError(node->loc, "trace solo soporta & sobre variables simples");
    }

    RuntimeValue r = eval(node->right.get());
    if (node->op == "-") lastValue = RuntimeValue::Int(-r.intValue);
    else if (node->op == "!") lastValue = RuntimeValue::Bool(!r.truthy());
    else if (node->op == "*") {
        if (!r.isPointer || r.pointsTo.empty()) throw SemanticError(node->loc, "puntero no inicializado durante desreferencia");
        lastValue = getPathValue(r.pointsTo, node->loc);
    }
}

void TraceVisitor::visit(IntExpr* node) { lastValue = RuntimeValue::Int(node->value); }
void TraceVisitor::visit(BoolExpr* node) { lastValue = RuntimeValue::Bool(node->value); }
void TraceVisitor::visit(CharExpr* node) { lastValue = RuntimeValue::Char(node->value); }
void TraceVisitor::visit(StringExpr* node) { lastValue = RuntimeValue::String(node->value); }
void TraceVisitor::visit(IdExpr* node) { lastValue = getVar(node->name, node->loc); }

void TraceVisitor::visit(ArrayAccessExpr* node) {
    std::vector<long> indices;
    for (auto& idxExpr : node->indices) {
        RuntimeValue idx = eval(idxExpr.get());
        indices.push_back(idx.intValue);
    }
    lastValue = getArrayElement(node->name, indices, node->loc);
}

void TraceVisitor::visit(FieldAccessExpr* node) {
    lastValue = getFieldValue(node->objectName, node->fieldName, node->loc);
}

void TraceVisitor::visit(NewExpr* node) {
    if (node->allocatedType.kind != TypeKind::Int || node->allocatedType.pointerDepth != 0 || node->allocatedType.isArray()) {
        throw SemanticError(node->loc, "trace solo soporta new int en esta versión");
    }
    std::string heapName = "heap#" + std::to_string(nextHeapId++);
    heapMemory[heapName] = RuntimeValue::Int(0);
    out << indent() << "Reservando memoria dinámica: " << heapName << " para int, valor inicial 0\n";
    lastValue = RuntimeValue::Pointer(heapName);
}

void TraceVisitor::visit(LambdaExpr* node) {
    lastValue = RuntimeValue::Lambda(node);
}

void TraceVisitor::visit(CallExpr* node) {
    std::vector<RuntimeValue> args;
    for (auto& arg : node->arguments) args.push_back(eval(arg.get()));

    FunctionDecl* fn = program->findFunction(node->callee);
    if (fn) {
        lastValue = callFunction(fn, args);
        return;
    }

    RuntimeValue callee = getVar(node->callee, node->loc);
    if (!callee.isLambda || !callee.lambda) {
        throw SemanticError(node->loc, "función o lambda no encontrada durante trace: " + node->callee);
    }
    lastValue = callLambda(callee.lambda, node->callee, args);
}

RuntimeValue TraceVisitor::defaultValueForType(const Type& type) const {
    if (type.pointerDepth > 0) return RuntimeValue::Pointer("");
    if (type.isArray()) return RuntimeValue::Array(type.kind, type.dimensions);
    if (type.kind == TypeKind::Struct) {
        RuntimeValue value = RuntimeValue::Struct(type.structName);
        auto it = structDefs.find(type.structName);
        if (it != structDefs.end()) {
            for (const auto& field : it->second->fields) value.fields[field.name] = defaultValueForType(field.type);
        }
        return value;
    }
    if (type.kind == TypeKind::Bool) return RuntimeValue::Bool(false);
    if (type.kind == TypeKind::Char) return RuntimeValue::Char('\0');
    if (type.kind == TypeKind::String) return RuntimeValue::String("");
    return RuntimeValue::Int(0);
}


RuntimeValue TraceVisitor::getFieldValue(const std::string& objectName, const std::string& fieldName, const SourceLocation& loc) {
    RuntimeValue obj = getVar(objectName, loc);
    if (!obj.isStruct) throw SemanticError(loc, "'" + objectName + "' no es un struct durante trace");
    auto it = obj.fields.find(fieldName);
    if (it == obj.fields.end()) throw SemanticError(loc, "campo no encontrado durante trace: " + objectName + "." + fieldName);
    return it->second;
}

void TraceVisitor::setFieldValue(const std::string& objectName, const std::string& fieldName, const RuntimeValue& value, const SourceLocation& loc) {
    for (auto it = envStack.rbegin(); it != envStack.rend(); ++it) {
        auto found = it->find(objectName);
        if (found != it->end()) {
            if (!found->second.isStruct) throw SemanticError(loc, "'" + objectName + "' no es un struct durante asignación");
            if (!found->second.fields.count(fieldName)) throw SemanticError(loc, "campo no encontrado durante asignación: " + objectName + "." + fieldName);
            found->second.fields[fieldName] = value;
            return;
        }
    }
    throw SemanticError(loc, "struct no encontrado durante asignación: " + objectName);
}

RuntimeValue TraceVisitor::getPathValue(const std::string& name, const SourceLocation& loc) {
    if (name.rfind("heap#", 0) == 0) {
        auto it = heapMemory.find(name);
        if (it == heapMemory.end()) throw SemanticError(loc, "bloque de memoria dinámica liberado o inexistente: " + name);
        return it->second;
    }
    auto dot = name.find('.');
    if (dot == std::string::npos) return getVar(name, loc);
    return getFieldValue(name.substr(0, dot), name.substr(dot + 1), loc);
}

RuntimeValue TraceVisitor::getArrayElement(const std::string& name, const std::vector<long>& indices, const SourceLocation& loc) {
    RuntimeValue arr = getVar(name, loc);
    if (!arr.isArray) throw SemanticError(loc, "'" + name + "' no es un arreglo durante trace");
    size_t linear = linearArrayIndex(arr, indices, name, loc);
    return arr.elements[linear];
}

void TraceVisitor::setArrayElement(const std::string& name, const std::vector<long>& indices, const RuntimeValue& value, const SourceLocation& loc) {
    for (auto it = envStack.rbegin(); it != envStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            if (!found->second.isArray) throw SemanticError(loc, "'" + name + "' no es un arreglo durante asignación");
            size_t linear = linearArrayIndex(found->second, indices, name, loc);
            found->second.elements[linear] = value;
            return;
        }
    }
    throw SemanticError(loc, "arreglo no encontrado durante asignación: " + name);
}

size_t TraceVisitor::linearArrayIndex(const RuntimeValue& arrayValue, const std::vector<long>& indices, const std::string& name, const SourceLocation& loc) const {
    if (indices.empty() || indices.size() != arrayValue.dimensions.size()) {
        throw SemanticError(loc, "trace requiere especificar todas las dimensiones de '" + name + "'");
    }
    long linear = 0;
    for (size_t i = 0; i < indices.size(); ++i) {
        long idx = indices[i];
        long dim = arrayValue.dimensions[i];
        if (idx < 0 || idx >= dim) {
            throw SemanticError(loc, "índice fuera de rango en trace array '" + name + "' dimensión " + std::to_string(i + 1) + ": " + std::to_string(idx));
        }
        long stride = 1;
        for (size_t j = i + 1; j < arrayValue.dimensions.size(); ++j) stride *= arrayValue.dimensions[j];
        linear += idx * stride;
    }
    return static_cast<size_t>(linear);
}

void TraceVisitor::printArray(const std::string& name, const std::string& title) {
    try {
        RuntimeValue arr = getVar(name, SourceLocation());
        out << indent() << title << ":\n";
        out << indent() << "  Arreglo " << name;
        for (int dim : arr.dimensions) out << "[" << dim << "]";
        out << "\n";

        if (arr.dimensions.size() == 2) {
            int rows = arr.dimensions[0];
            int cols = arr.dimensions[1];
            for (int i = 0; i < rows; ++i) {
                out << indent() << "  fila " << i << ":";
                for (int j = 0; j < cols; ++j) {
                    out << " " << arr.elements[static_cast<size_t>(i * cols + j)].toString();
                }
                out << "\n";
            }
            return;
        }

        out << indent() << "  índice:";
        for (size_t i = 0; i < arr.elements.size(); ++i) out << " " << i;
        out << "\n";
        out << indent() << "  valor :";
        for (const auto& value : arr.elements) out << " " << value.toString();
        out << "\n";
    } catch (...) {
        out << indent() << title << ": " << name << " = <no disponible>\n";
    }
}

RuntimeValue TraceVisitor::getVar(const std::string& name, const SourceLocation& loc) {
    for (auto it = envStack.rbegin(); it != envStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return found->second;
    }
    throw SemanticError(loc, "variable no encontrada durante trace: " + name);
}

void TraceVisitor::setVar(const std::string& name, const RuntimeValue& value, const SourceLocation& loc) {
    for (auto it = envStack.rbegin(); it != envStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            found->second = value;
            return;
        }
    }
    throw SemanticError(loc, "variable no encontrada durante asignación: " + name);
}

void TraceVisitor::setPathValue(const std::string& name, const RuntimeValue& value, const SourceLocation& loc) {
    if (name.rfind("heap#", 0) == 0) {
        auto it = heapMemory.find(name);
        if (it == heapMemory.end()) throw SemanticError(loc, "bloque de memoria dinámica liberado o inexistente: " + name);
        it->second = value;
        return;
    }
    auto dot = name.find('.');
    if (dot == std::string::npos) {
        setVar(name, value, loc);
        return;
    }
    setFieldValue(name.substr(0, dot), name.substr(dot + 1), value, loc);
}

void TraceVisitor::declareVar(const std::string& name, const RuntimeValue& value) {
    if (envStack.empty()) envStack.emplace_back();
    envStack.back()[name] = value;
}

void TraceVisitor::printMemoryState(const std::vector<std::string>& names, const std::string& title) {
    out << indent() << title << ":\n";
    if (names.empty()) {
        out << indent() << "  <sin variables observadas>\n";
        return;
    }
    for (const auto& name : names) {
        try {
            RuntimeValue value = getPathValue(name, SourceLocation());
            out << indent() << "  " << name << " = " << value.toString();
            if (value.isPointer && !value.pointsTo.empty()) {
                RuntimeValue target = getPathValue(value.pointsTo, SourceLocation());
                out << "  -> apunta a " << value.pointsTo << " = " << target.toString();
            }
            out << "\n";
        } catch (...) {
            out << indent() << "  " << name << " = <no disponible>\n";
        }
    }
}

void TraceVisitor::printWatchedMemoryState(const std::string& title) {
    std::vector<std::string> names(watchedMemoryNames.begin(), watchedMemoryNames.end());
    std::sort(names.begin(), names.end());
    printMemoryState(names, title);
}

void TraceVisitor::printWatchedVariables(const std::unordered_set<std::string>& names, const std::string& title) {
    out << indent() << title << ":\n";
    for (const auto& name : names) {
        try { out << indent() << "  " << name << " = " << getPathValue(name, SourceLocation()).toString() << "\n"; }
        catch (...) { out << indent() << "  " << name << " = <no disponible>\n"; }
    }
}


void TraceVisitor::printCurrentStackFrame(const std::string& title) {
    out << indent() << title << ":\n";
    if (!callStack.empty()) out << indent() << "  función actual: " << callStack.back() << "\n";
    printCallStack("  pila");

    std::unordered_map<std::string, RuntimeValue> visible;
    for (const auto& scope : envStack) {
        for (const auto& entry : scope) visible[entry.first] = entry.second;
    }

    if (visible.empty()) {
        out << indent() << "  variables visibles: <ninguna>\n";
        return;
    }

    std::vector<std::string> names;
    for (const auto& entry : visible) names.push_back(entry.first);
    std::sort(names.begin(), names.end());

    out << indent() << "  variables visibles:\n";
    for (const auto& name : names) {
        out << indent() << "    " << name << " = " << visible[name].toString() << "\n";
    }
}

void TraceVisitor::printCallStack(const std::string& title) {
    out << indent() << title << ": ";
    if (callStack.empty()) {
        out << "<vacía>\n";
        return;
    }
    for (size_t i = 0; i < callStack.size(); ++i) {
        if (i > 0) out << " -> ";
        out << callStack[i];
    }
    out << "\n";
}

std::string TraceVisitor::exprToString(const Expr* expr) const {
    if (!expr) return "<expr>";

    if (auto n = dynamic_cast<const IntExpr*>(expr)) return std::to_string(n->value);
    if (auto n = dynamic_cast<const BoolExpr*>(expr)) return n->value ? "true" : "false";
    if (auto n = dynamic_cast<const CharExpr*>(expr)) return std::string("'") + n->value + "'";
    if (auto n = dynamic_cast<const StringExpr*>(expr)) return "\"" + n->value + "\"";
    if (auto n = dynamic_cast<const IdExpr*>(expr)) return n->name;
    if (auto n = dynamic_cast<const ArrayAccessExpr*>(expr)) {
        std::string text = n->name;
        for (const auto& idx : n->indices) text += "[" + exprToString(idx.get()) + "]";
        return text;
    }
    if (auto n = dynamic_cast<const FieldAccessExpr*>(expr)) return n->fullName();
    if (auto n = dynamic_cast<const UnaryExpr*>(expr)) return n->op + exprToString(n->right.get());
    if (auto n = dynamic_cast<const BinaryExpr*>(expr)) {
        return exprToString(n->left.get()) + " " + n->op + " " + exprToString(n->right.get());
    }
    if (auto n = dynamic_cast<const NewExpr*>(expr)) return "new " + n->allocatedType.toString();
    if (auto n = dynamic_cast<const LambdaExpr*>(expr)) return "lambda " + n->returnType.toString() + "(...)";
    if (auto n = dynamic_cast<const CallExpr*>(expr)) {
        std::string text = n->callee + "(";
        for (size_t i = 0; i < n->arguments.size(); ++i) {
            if (i > 0) text += ", ";
            text += exprToString(n->arguments[i].get());
        }
        text += ")";
        return text;
    }

    return "<expr>";
}

std::string TraceVisitor::indent() const {
    return std::string(callStack.size() * 2, ' ');
}
