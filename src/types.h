#ifndef EDUTRACE_TYPES_H
#define EDUTRACE_TYPES_H

#include <string>
#include <vector>
#include <sstream>
#include <utility>

// Tipo semántico del lenguaje EduTrace.
enum class TypeKind {
    Int,
    Bool,
    Char,
    String,
    Struct,
    Generic,
    Lambda,
    Void,
    Unknown,
    Error
};

struct Type {
    TypeKind kind = TypeKind::Unknown;
    int pointerDepth = 0;
    std::string structName;
    std::string genericName;
    std::vector<int> dimensions;
    std::vector<Type> lambdaParams;
    Type* lambdaReturn = nullptr;

    Type() = default;
    explicit Type(TypeKind k) : kind(k) {}

    static Type Int() { return Type(TypeKind::Int); }
    static Type Bool() { return Type(TypeKind::Bool); }
    static Type Char() { return Type(TypeKind::Char); }
    static Type String() { return Type(TypeKind::String); }
    static Type Void() { return Type(TypeKind::Void); }
    static Type Struct(std::string name) { Type t(TypeKind::Struct); t.structName = std::move(name); return t; }
    static Type Generic(std::string name) { Type t(TypeKind::Generic); t.genericName = std::move(name); return t; }
    static Type Lambda(std::vector<Type> params, Type ret) {
        Type t(TypeKind::Lambda);
        t.lambdaParams = std::move(params);
        t.lambdaReturn = new Type(std::move(ret));
        return t;
    }
    static Type Unknown() { return Type(TypeKind::Unknown); }
    static Type Error() { return Type(TypeKind::Error); }

    bool isNumeric() const {
        return kind == TypeKind::Int || kind == TypeKind::Char;
    }

    bool isBoolean() const {
        return kind == TypeKind::Bool;
    }

    bool isArray() const {
        return !dimensions.empty();
    }

    Type elementType() const {
        Type t = *this;
        if (!t.dimensions.empty()) t.dimensions.erase(t.dimensions.begin());
        return t;
    }

    long totalElements() const {
        if (dimensions.empty()) return 1;
        long total = 1;
        for (int dim : dimensions) total *= dim;
        return total;
    }

    bool isAssignableFrom(const Type& other) const {
        if (kind == TypeKind::Error || other.kind == TypeKind::Error) return true;
        if (kind == other.kind && pointerDepth == other.pointerDepth && dimensions == other.dimensions && structName == other.structName && genericName == other.genericName) {
            if (kind != TypeKind::Lambda) return true;
            if (lambdaParams.size() != other.lambdaParams.size()) return false;
            for (size_t i = 0; i < lambdaParams.size(); ++i) {
                if (lambdaParams[i].toString() != other.lambdaParams[i].toString()) return false;
            }
            if (!lambdaReturn || !other.lambdaReturn) return false;
            return lambdaReturn->toString() == other.lambdaReturn->toString();
        }

        // Promoción básica requerida por el proyecto: char -> int y bool -> int.
        if (kind == TypeKind::Int && other.kind == TypeKind::Char) return true;
        if (kind == TypeKind::Int && other.kind == TypeKind::Bool) return true;

        return false;
    }

    std::string toString() const {
        std::ostringstream out;
        switch (kind) {
            case TypeKind::Int: out << "int"; break;
            case TypeKind::Bool: out << "bool"; break;
            case TypeKind::Char: out << "char"; break;
            case TypeKind::String: out << "string"; break;
            case TypeKind::Struct: out << structName; break;
            case TypeKind::Generic: out << genericName; break;
            case TypeKind::Lambda:
                out << "lambda ";
                if (lambdaReturn) out << lambdaReturn->toString();
                else out << "unknown";
                out << "(";
                for (size_t i = 0; i < lambdaParams.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << lambdaParams[i].toString();
                }
                out << ")";
                break;
            case TypeKind::Void: out << "void"; break;
            case TypeKind::Unknown: out << "unknown"; break;
            case TypeKind::Error: out << "error"; break;
        }
        for (int i = 0; i < pointerDepth; ++i) out << "*";
        for (int dim : dimensions) out << "[" << dim << "]";
        return out.str();
    }
};

inline bool operator==(const Type& a, const Type& b) {
    if (a.kind != b.kind || a.pointerDepth != b.pointerDepth || a.dimensions != b.dimensions || a.structName != b.structName || a.genericName != b.genericName) return false;
    if (a.kind == TypeKind::Lambda) {
        if (a.lambdaParams.size() != b.lambdaParams.size()) return false;
        for (size_t i = 0; i < a.lambdaParams.size(); ++i) if (!(a.lambdaParams[i] == b.lambdaParams[i])) return false;
        if (!a.lambdaReturn || !b.lambdaReturn) return a.lambdaReturn == b.lambdaReturn;
        return *a.lambdaReturn == *b.lambdaReturn;
    }
    return true;
}

inline bool operator!=(const Type& a, const Type& b) {
    return !(a == b);
}

#endif
