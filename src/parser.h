#ifndef EDUTRACE_PARSER_H
#define EDUTRACE_PARSER_H

#include <memory>
#include <vector>
#include "ast.h"
#include "scanner.h"

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    std::unique_ptr<Program> parse();

private:
    std::vector<Token> tokens;
    size_t current = 0;

    std::unique_ptr<StructDecl> structDecl();
    std::unique_ptr<FunctionDecl> functionDecl();
    std::vector<Param> parameters();
    Type parseType();
    Type parseTypeArgument();
    std::unique_ptr<BlockStmt> block();
    std::unique_ptr<Stmt> declarationOrStatement();
    std::unique_ptr<Stmt> varDeclaration(bool inferred);
    std::unique_ptr<Stmt> statement();
    std::unique_ptr<Stmt> ifStatement();
    std::unique_ptr<Stmt> whileStatement();
    std::unique_ptr<Stmt> returnStatement();
    std::unique_ptr<Stmt> printStatement();
    std::unique_ptr<Stmt> traceStatement();
    std::unique_ptr<Stmt> deleteStatement();
    std::unique_ptr<Stmt> assignmentOrExprStatement();
    std::string traceName();

    std::unique_ptr<Expr> expression();
    std::unique_ptr<Expr> orExpr();
    std::unique_ptr<Expr> andExpr();
    std::unique_ptr<Expr> equality();
    std::unique_ptr<Expr> comparison();
    std::unique_ptr<Expr> term();
    std::unique_ptr<Expr> factor();
    std::unique_ptr<Expr> unary();
    std::unique_ptr<Expr> call();
    std::unique_ptr<Expr> primary();

    bool match(TokenType type);
    bool matchAny(std::initializer_list<TokenType> types);
    bool check(TokenType type) const;
    Token advance();
    bool isAtEnd() const;
    Token peek() const;
    Token previous() const;
    Token consume(TokenType type, const std::string& message);
    bool isArrayAssignmentStart() const;
    bool isPointerAssignmentStart() const;
    bool isFieldAssignmentStart() const;
};

#endif
