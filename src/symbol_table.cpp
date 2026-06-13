#include "symbol_table.h"

SymbolTable::SymbolTable() {
    enterScope();
}

void SymbolTable::enterScope() {
    scopes.emplace_back();
}

void SymbolTable::exitScope() {
    if (!scopes.empty()) scopes.pop_back();
}

bool SymbolTable::declare(const std::string& name, const Type& type, SourceLocation loc, bool isParameter) {
    if (scopes.empty()) enterScope();
    auto& current = scopes.back();
    if (current.count(name)) return false;
    current[name] = Symbol{name, type, isParameter, 0, loc};
    return true;
}

Symbol* SymbolTable::resolve(const std::string& name) {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

const Symbol* SymbolTable::resolve(const std::string& name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) return &found->second;
    }
    return nullptr;
}

int SymbolTable::depth() const {
    return static_cast<int>(scopes.size());
}
