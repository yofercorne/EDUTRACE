#ifndef EDUTRACE_ERRORS_H
#define EDUTRACE_ERRORS_H

#include <stdexcept>
#include <string>

struct SourceLocation {
    int line = 1;
    int column = 1;

    SourceLocation() = default;
    SourceLocation(int l, int c) : line(l), column(c) {}

    std::string toString() const {
        return "línea " + std::to_string(line) + ", columna " + std::to_string(column);
    }
};

class CompilerError : public std::runtime_error {
public:
    explicit CompilerError(const std::string& message)
        : std::runtime_error(message) {}
};

class LexicalError : public CompilerError {
public:
    LexicalError(const SourceLocation& loc, const std::string& message)
        : CompilerError("Error léxico en " + loc.toString() + ": " + message) {}
};

class SyntaxError : public CompilerError {
public:
    SyntaxError(const SourceLocation& loc, const std::string& message)
        : CompilerError("Error sintáctico en " + loc.toString() + ": " + message) {}
};

class SemanticError : public CompilerError {
public:
    SemanticError(const SourceLocation& loc, const std::string& message)
        : CompilerError("Error semántico en " + loc.toString() + ": " + message) {}
};

#endif
