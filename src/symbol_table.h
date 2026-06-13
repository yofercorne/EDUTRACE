#ifndef EDUTRACE_SYMBOL_TABLE_H
#define EDUTRACE_SYMBOL_TABLE_H

#include <string>
#include <unordered_map>
#include <vector>
#include "types.h"
#include "errors.h"

struct Symbol {
    std::string name;
    Type type;
    bool isParameter = false;
    int offset = 0;
    SourceLocation loc;
};

class SymbolTable {
public:
    SymbolTable();

    void enterScope();
    void exitScope();

    bool declare(const std::string& name, const Type& type, SourceLocation loc, bool isParameter = false);
    Symbol* resolve(const std::string& name);
    const Symbol* resolve(const std::string& name) const;

    int depth() const;

private:
    std::vector<std::unordered_map<std::string, Symbol>> scopes;
};

#endif
