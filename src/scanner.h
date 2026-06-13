#ifndef EDUTRACE_SCANNER_H
#define EDUTRACE_SCANNER_H

#include <string>
#include <vector>
#include <unordered_map>
#include "errors.h"

enum class TokenType {
    // Un carácter
    LeftParen, RightParen, LeftBrace, RightBrace, LeftBracket, RightBracket,
    Comma, Dot, Semicolon, Colon,
    Plus, Minus, Star, Slash, Percent,
    Ampersand,

    // Uno o dos caracteres
    Bang, BangEqual,
    Equal, EqualEqual,
    Greater, GreaterEqual,
    Less, LessEqual,
    AndAnd, OrOr,

    // Literales
    Identifier, Number, StringLiteral, CharLiteral,

    // Palabras reservadas
    Fun, Var, Let, If, Else, While, For, Return, Print,
    Struct, True, False,
    Int, Bool, Char, StringType, Void,
    New, Delete, Lambda,

    // Directivas educativas
    Trace, Variables, Loop, Branch, Stack, Recursion, Array, Memory,

    EndOfFile
};

struct Token {
    TokenType type;
    std::string lexeme;
    SourceLocation loc;

    Token(TokenType t, std::string lx, SourceLocation l)
        : type(t), lexeme(std::move(lx)), loc(l) {}
};

std::string tokenTypeName(TokenType type);

class Scanner {
public:
    explicit Scanner(std::string source);
    std::vector<Token> scanTokens();

private:
    std::string source;
    std::vector<Token> tokens;
    size_t start = 0;
    size_t current = 0;
    int line = 1;
    int column = 1;
    int tokenColumn = 1;

    std::unordered_map<std::string, TokenType> keywords;

    bool isAtEnd() const;
    char advance();
    char peek() const;
    char peekNext() const;
    bool match(char expected);

    void scanToken();
    void addToken(TokenType type);
    void stringLiteral();
    void charLiteral();
    void number();
    void identifier();

    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
};

#endif
