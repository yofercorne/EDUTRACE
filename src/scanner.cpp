#include "scanner.h"
#include <cctype>
#include <sstream>

using std::string;
using std::vector;

Scanner::Scanner(std::string src) : source(std::move(src)) {
    keywords = {
        {"fun", TokenType::Fun}, {"var", TokenType::Var}, {"let", TokenType::Let},
        {"if", TokenType::If}, {"else", TokenType::Else}, {"while", TokenType::While},
        {"for", TokenType::For}, {"return", TokenType::Return}, {"print", TokenType::Print},
        {"struct", TokenType::Struct}, {"true", TokenType::True}, {"false", TokenType::False},
        {"int", TokenType::Int}, {"bool", TokenType::Bool}, {"char", TokenType::Char},
        {"string", TokenType::StringType}, {"void", TokenType::Void},
        {"new", TokenType::New}, {"delete", TokenType::Delete}, {"lambda", TokenType::Lambda},
        {"trace", TokenType::Trace}, {"variables", TokenType::Variables}, {"loop", TokenType::Loop},
        {"branch", TokenType::Branch}, {"stack", TokenType::Stack}, {"recursion", TokenType::Recursion},
        {"array", TokenType::Array}, {"memory", TokenType::Memory}
    };
}

vector<Token> Scanner::scanTokens() {
    while (!isAtEnd()) {
        start = current;
        tokenColumn = column;
        scanToken();
    }
    tokens.emplace_back(TokenType::EndOfFile, "", SourceLocation(line, column));
    return tokens;
}

bool Scanner::isAtEnd() const {
    return current >= source.size();
}

char Scanner::advance() {
    char c = source[current++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

char Scanner::peek() const {
    if (isAtEnd()) return '\0';
    return source[current];
}

char Scanner::peekNext() const {
    if (current + 1 >= source.size()) return '\0';
    return source[current + 1];
}

bool Scanner::match(char expected) {
    if (isAtEnd()) return false;
    if (source[current] != expected) return false;
    advance();
    return true;
}

void Scanner::addToken(TokenType type) {
    string text = source.substr(start, current - start);
    tokens.emplace_back(type, text, SourceLocation(line, tokenColumn));
}

void Scanner::scanToken() {
    char c = advance();
    switch (c) {
        case '(': addToken(TokenType::LeftParen); break;
        case ')': addToken(TokenType::RightParen); break;
        case '{': addToken(TokenType::LeftBrace); break;
        case '}': addToken(TokenType::RightBrace); break;
        case '[': addToken(TokenType::LeftBracket); break;
        case ']': addToken(TokenType::RightBracket); break;
        case ',': addToken(TokenType::Comma); break;
        case '.': addToken(TokenType::Dot); break;
        case ';': addToken(TokenType::Semicolon); break;
        case ':': addToken(TokenType::Colon); break;
        case '+': addToken(TokenType::Plus); break;
        case '-': addToken(TokenType::Minus); break;
        case '*': addToken(TokenType::Star); break;
        case '%': addToken(TokenType::Percent); break;
        case '&': addToken(match('&') ? TokenType::AndAnd : TokenType::Ampersand); break;
        case '|':
            if (match('|')) addToken(TokenType::OrOr);
            else throw LexicalError(SourceLocation(line, tokenColumn), "se esperaba '|' para formar '||'");
            break;
        case '!': addToken(match('=') ? TokenType::BangEqual : TokenType::Bang); break;
        case '=': addToken(match('=') ? TokenType::EqualEqual : TokenType::Equal); break;
        case '<': addToken(match('=') ? TokenType::LessEqual : TokenType::Less); break;
        case '>': addToken(match('=') ? TokenType::GreaterEqual : TokenType::Greater); break;
        case '/':
            if (match('/')) {
                while (peek() != '\n' && !isAtEnd()) advance();
            } else if (match('*')) {
                while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) advance();
                if (isAtEnd()) throw LexicalError(SourceLocation(line, tokenColumn), "comentario de bloque sin cerrar");
                advance(); // '*'
                advance(); // '/'
            } else {
                addToken(TokenType::Slash);
            }
            break;
        case ' ':
        case '\r':
        case '\t':
        case '\n':
            break;
        case '"': stringLiteral(); break;
        case '\'': charLiteral(); break;
        default:
            if (std::isdigit(static_cast<unsigned char>(c))) number();
            else if (isAlpha(c)) identifier();
            else throw LexicalError(SourceLocation(line, tokenColumn), string("carácter inválido '") + c + "'");
            break;
    }
}

void Scanner::stringLiteral() {
    while (peek() != '"' && !isAtEnd()) advance();
    if (isAtEnd()) throw LexicalError(SourceLocation(line, tokenColumn), "cadena sin cerrar");
    advance(); // cierre
    string value = source.substr(start + 1, current - start - 2);
    tokens.emplace_back(TokenType::StringLiteral, value, SourceLocation(line, tokenColumn));
}


void Scanner::charLiteral() {
    if (isAtEnd() || peek() == '\n') {
        throw LexicalError(SourceLocation(line, tokenColumn), "literal char sin cerrar");
    }

    char value;
    if (peek() == '\\') {
        advance(); // consume '\\'
        if (isAtEnd()) throw LexicalError(SourceLocation(line, tokenColumn), "escape incompleto en literal char");
        char escaped = advance();
        switch (escaped) {
            case 'n': value = '\n'; break;
            case 't': value = '\t'; break;
            case '\\': value = '\\'; break;
            case '\'': value = '\''; break;
            default:
                throw LexicalError(SourceLocation(line, tokenColumn), std::string("escape inválido en char: ") + escaped);
        }
    } else {
        value = advance();
    }

    if (!match('\'')) {
        throw LexicalError(SourceLocation(line, tokenColumn), "literal char debe tener exactamente un carácter y cerrar con comilla simple");
    }

    tokens.emplace_back(TokenType::CharLiteral, std::string(1, value), SourceLocation(line, tokenColumn));
}

void Scanner::number() {
    while (std::isdigit(static_cast<unsigned char>(peek()))) advance();
    addToken(TokenType::Number);
}

void Scanner::identifier() {
    while (isAlphaNumeric(peek())) advance();
    string text = source.substr(start, current - start);
    auto it = keywords.find(text);
    if (it != keywords.end()) addToken(it->second);
    else addToken(TokenType::Identifier);
}

bool Scanner::isAlpha(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool Scanner::isAlphaNumeric(char c) {
    return isAlpha(c) || std::isdigit(static_cast<unsigned char>(c));
}

string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::LeftParen: return "LEFT_PAREN";
        case TokenType::RightParen: return "RIGHT_PAREN";
        case TokenType::LeftBrace: return "LEFT_BRACE";
        case TokenType::RightBrace: return "RIGHT_BRACE";
        case TokenType::LeftBracket: return "LEFT_BRACKET";
        case TokenType::RightBracket: return "RIGHT_BRACKET";
        case TokenType::Comma: return "COMMA";
        case TokenType::Dot: return "DOT";
        case TokenType::Semicolon: return "SEMICOLON";
        case TokenType::Colon: return "COLON";
        case TokenType::Plus: return "PLUS";
        case TokenType::Minus: return "MINUS";
        case TokenType::Star: return "STAR";
        case TokenType::Slash: return "SLASH";
        case TokenType::Percent: return "PERCENT";
        case TokenType::Ampersand: return "AMPERSAND";
        case TokenType::Bang: return "BANG";
        case TokenType::BangEqual: return "BANG_EQUAL";
        case TokenType::Equal: return "EQUAL";
        case TokenType::EqualEqual: return "EQUAL_EQUAL";
        case TokenType::Greater: return "GREATER";
        case TokenType::GreaterEqual: return "GREATER_EQUAL";
        case TokenType::Less: return "LESS";
        case TokenType::LessEqual: return "LESS_EQUAL";
        case TokenType::AndAnd: return "AND_AND";
        case TokenType::OrOr: return "OR_OR";
        case TokenType::Identifier: return "IDENTIFIER";
        case TokenType::Number: return "NUMBER";
        case TokenType::StringLiteral: return "STRING_LITERAL";
        case TokenType::CharLiteral: return "CHAR_LITERAL";
        case TokenType::Fun: return "FUN";
        case TokenType::Var: return "VAR";
        case TokenType::Let: return "LET";
        case TokenType::If: return "IF";
        case TokenType::Else: return "ELSE";
        case TokenType::While: return "WHILE";
        case TokenType::For: return "FOR";
        case TokenType::Return: return "RETURN";
        case TokenType::Print: return "PRINT";
        case TokenType::Struct: return "STRUCT";
        case TokenType::True: return "TRUE";
        case TokenType::False: return "FALSE";
        case TokenType::Int: return "INT";
        case TokenType::Bool: return "BOOL";
        case TokenType::Char: return "CHAR";
        case TokenType::StringType: return "STRING";
        case TokenType::Void: return "VOID";
        case TokenType::New: return "NEW";
        case TokenType::Delete: return "DELETE";
        case TokenType::Lambda: return "LAMBDA";
        case TokenType::Trace: return "TRACE";
        case TokenType::Variables: return "VARIABLES";
        case TokenType::Loop: return "LOOP";
        case TokenType::Branch: return "BRANCH";
        case TokenType::Stack: return "STACK";
        case TokenType::Recursion: return "RECURSION";
        case TokenType::Array: return "ARRAY";
        case TokenType::Memory: return "MEMORY";
        case TokenType::EndOfFile: return "EOF";
    }
    return "UNKNOWN";
}
