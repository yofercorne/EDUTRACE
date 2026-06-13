#include "parser.h"
#include <cstdlib>
#include <utility>

Parser::Parser(std::vector<Token> toks) : tokens(std::move(toks)) {}

std::unique_ptr<Program> Parser::parse() {
    auto program = std::make_unique<Program>();
    while (!isAtEnd()) {
        if (match(TokenType::Struct)) program->structs.push_back(structDecl());
        else program->functions.push_back(functionDecl());
    }
    return program;
}

std::unique_ptr<StructDecl> Parser::structDecl() {
    Token structTok = previous();
    Token name = consume(TokenType::Identifier, "se esperaba nombre de struct");
    consume(TokenType::LeftBrace, "se esperaba '{' después del nombre de struct");
    std::vector<Param> fields;
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        Type t = parseType();
        Token field = consume(TokenType::Identifier, "se esperaba nombre de campo");
        while (match(TokenType::LeftBracket)) {
            Token size = consume(TokenType::Number, "se esperaba tamaño del arreglo en campo");
            int dimension = std::stoi(size.lexeme);
            if (dimension <= 0) throw SyntaxError(size.loc, "el tamaño del arreglo debe ser mayor que cero");
            t.dimensions.push_back(dimension);
            consume(TokenType::RightBracket, "se esperaba ']' después del tamaño");
        }
        fields.push_back(Param{t, field.lexeme, field.loc});
        consume(TokenType::Semicolon, "se esperaba ';' después del campo");
    }
    consume(TokenType::RightBrace, "se esperaba '}' al cerrar struct");
    return std::make_unique<StructDecl>(name.lexeme, std::move(fields), structTok.loc);
}

std::unique_ptr<FunctionDecl> Parser::functionDecl() {
    Token funTok = consume(TokenType::Fun, "se esperaba 'fun' al inicio de una función");
    Type ret = parseType();
    Token name = consume(TokenType::Identifier, "se esperaba nombre de función");
    std::string templateParam;
    if (match(TokenType::Less)) {
        Token tname = consume(TokenType::Identifier, "se esperaba parámetro de template después de '<'");
        templateParam = tname.lexeme;
        consume(TokenType::Greater, "se esperaba '>' después del parámetro de template");
    }
    consume(TokenType::LeftParen, "se esperaba '(' después del nombre de función");
    auto params = parameters();
    consume(TokenType::RightParen, "se esperaba ')' después de los parámetros");
    auto body = block();
    return std::make_unique<FunctionDecl>(ret, name.lexeme, std::move(params), std::move(body), funTok.loc, templateParam);
}

std::vector<Param> Parser::parameters() {
    std::vector<Param> params;
    if (check(TokenType::RightParen)) return params;
    do {
        Type t = parseType();
        Token name = consume(TokenType::Identifier, "se esperaba nombre de parámetro");
        params.push_back(Param{t, name.lexeme, name.loc});
    } while (match(TokenType::Comma));
    return params;
}

Type Parser::parseType() {
    Type type;
    if (match(TokenType::Int)) type = Type::Int();
    else if (match(TokenType::Bool)) type = Type::Bool();
    else if (match(TokenType::Char)) type = Type::Char();
    else if (match(TokenType::StringType)) type = Type::String();
    else if (match(TokenType::Void)) type = Type::Void();
    else if (match(TokenType::Identifier)) {
        std::string name = previous().lexeme;
        if (name == "T") type = Type::Generic(name);
        else type = Type::Struct(name);
    }
    else throw SyntaxError(peek().loc, "se esperaba un tipo");

    while (match(TokenType::Star)) type.pointerDepth++;
    while (match(TokenType::LeftBracket)) {
        Token size = consume(TokenType::Number, "se esperaba tamaño del arreglo");
        type.dimensions.push_back(std::stoi(size.lexeme));
        consume(TokenType::RightBracket, "se esperaba ']' después del tamaño");
    }
    return type;
}


Type Parser::parseTypeArgument() {
    return parseType();
}

std::unique_ptr<BlockStmt> Parser::block() {
    Token open = consume(TokenType::LeftBrace, "se esperaba '{'");
    auto b = std::make_unique<BlockStmt>(open.loc);
    while (!check(TokenType::RightBrace) && !isAtEnd()) {
        b->statements.push_back(declarationOrStatement());
    }
    consume(TokenType::RightBrace, "se esperaba '}' al cerrar el bloque");
    return b;
}

std::unique_ptr<Stmt> Parser::declarationOrStatement() {
    if (match(TokenType::Var)) return varDeclaration(false);
    if (match(TokenType::Let)) return varDeclaration(true);
    return statement();
}

std::unique_ptr<Stmt> Parser::varDeclaration(bool inferred) {
    SourceLocation loc = previous().loc;
    Type type = inferred ? Type::Unknown() : parseType();
    Token name = consume(TokenType::Identifier, "se esperaba nombre de variable");
    while (!inferred && match(TokenType::LeftBracket)) {
        Token size = consume(TokenType::Number, "se esperaba tamaño del arreglo");
        int dimension = std::stoi(size.lexeme);
        if (dimension <= 0) throw SyntaxError(size.loc, "el tamaño del arreglo debe ser mayor que cero");
        type.dimensions.push_back(dimension);
        consume(TokenType::RightBracket, "se esperaba ']' después del tamaño");
    }
    std::unique_ptr<Expr> init;
    if (match(TokenType::Equal)) init = expression();
    if (inferred && !init) throw SyntaxError(name.loc, "'let' requiere inicializador para inferir el tipo");
    consume(TokenType::Semicolon, "se esperaba ';' después de la declaración");
    return std::make_unique<VarDeclStmt>(type, name.lexeme, std::move(init), inferred, loc);
}

std::unique_ptr<Stmt> Parser::statement() {
    if (match(TokenType::If)) return ifStatement();
    if (match(TokenType::While)) return whileStatement();
    if (match(TokenType::Return)) return returnStatement();
    if (match(TokenType::Print)) return printStatement();
    if (match(TokenType::Trace)) return traceStatement();
    if (match(TokenType::Delete)) return deleteStatement();
    if (check(TokenType::LeftBrace)) return block();
    return assignmentOrExprStatement();
}

std::unique_ptr<Stmt> Parser::ifStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LeftParen, "se esperaba '(' después de if");
    auto cond = expression();
    consume(TokenType::RightParen, "se esperaba ')' después de la condición");
    auto thenB = block();
    std::unique_ptr<BlockStmt> elseB;
    if (match(TokenType::Else)) elseB = block();
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenB), std::move(elseB), loc);
}

std::unique_ptr<Stmt> Parser::whileStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LeftParen, "se esperaba '(' después de while");
    auto cond = expression();
    consume(TokenType::RightParen, "se esperaba ')' después de la condición");
    auto body = block();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::returnStatement() {
    SourceLocation loc = previous().loc;
    std::unique_ptr<Expr> value;
    if (!check(TokenType::Semicolon)) value = expression();
    consume(TokenType::Semicolon, "se esperaba ';' después de return");
    return std::make_unique<ReturnStmt>(std::move(value), loc);
}

std::unique_ptr<Stmt> Parser::printStatement() {
    SourceLocation loc = previous().loc;
    consume(TokenType::LeftParen, "se esperaba '(' después de print");
    auto expr = expression();
    consume(TokenType::RightParen, "se esperaba ')' después de print");
    consume(TokenType::Semicolon, "se esperaba ';' después de print");
    return std::make_unique<PrintStmt>(std::move(expr), loc);
}

std::unique_ptr<Stmt> Parser::traceStatement() {
    SourceLocation loc = previous().loc;
    TraceKind kind;
    if (match(TokenType::Variables)) kind = TraceKind::Variables;
    else if (match(TokenType::Loop)) kind = TraceKind::Loop;
    else if (match(TokenType::Branch)) kind = TraceKind::Branch;
    else if (match(TokenType::Stack)) kind = TraceKind::Stack;
    else if (match(TokenType::Recursion)) kind = TraceKind::Recursion;
    else if (match(TokenType::Array)) kind = TraceKind::Array;
    else if (match(TokenType::Memory)) kind = TraceKind::Memory;
    else throw SyntaxError(peek().loc, "se esperaba tipo de trace: variables, loop, branch, stack, recursion, array o memory");

    std::vector<std::string> names;
    if (match(TokenType::LeftParen)) {
        if (!check(TokenType::RightParen)) {
            do {
                names.push_back(traceName());
            } while (match(TokenType::Comma));
        }
        consume(TokenType::RightParen, "se esperaba ')' en trace");
    }
    consume(TokenType::Semicolon, "se esperaba ';' después de trace");
    return std::make_unique<TraceStmt>(kind, std::move(names), loc);
}

std::unique_ptr<Stmt> Parser::deleteStatement() {
    SourceLocation loc = previous().loc;
    Token name = consume(TokenType::Identifier, "se esperaba nombre de puntero después de delete");
    consume(TokenType::Semicolon, "se esperaba ';' después de delete");
    return std::make_unique<DeleteStmt>(name.lexeme, loc);
}

std::unique_ptr<Stmt> Parser::assignmentOrExprStatement() {
    SourceLocation loc = peek().loc;
    if (isPointerAssignmentStart()) {
        advance(); // *
        Token ptr = consume(TokenType::Identifier, "se esperaba nombre de puntero después de '*'");
        consume(TokenType::Equal, "se esperaba '=' en asignación por puntero");
        auto value = expression();
        consume(TokenType::Semicolon, "se esperaba ';' después de la asignación por puntero");
        return std::make_unique<PointerAssignStmt>(ptr.lexeme, std::move(value), loc);
    }
    if (isFieldAssignmentStart()) {
        Token obj = advance();
        consume(TokenType::Dot, "se esperaba '.' en asignación a campo");
        Token field = consume(TokenType::Identifier, "se esperaba nombre de campo");
        consume(TokenType::Equal, "se esperaba '=' en asignación a campo");
        auto value = expression();
        consume(TokenType::Semicolon, "se esperaba ';' después de la asignación");
        return std::make_unique<FieldAssignStmt>(obj.lexeme, field.lexeme, std::move(value), loc);
    }
    if (check(TokenType::Identifier) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::Equal) {
        Token name = advance();
        consume(TokenType::Equal, "se esperaba '='");
        auto value = expression();
        consume(TokenType::Semicolon, "se esperaba ';' después de la asignación");
        return std::make_unique<AssignStmt>(name.lexeme, std::move(value), loc);
    }
    if (isArrayAssignmentStart()) {
        Token name = advance();
        std::vector<std::unique_ptr<Expr>> indices;
        do {
            consume(TokenType::LeftBracket, "se esperaba '[' en asignación a arreglo");
            indices.push_back(expression());
            consume(TokenType::RightBracket, "se esperaba ']' después del índice");
        } while (check(TokenType::LeftBracket));
        consume(TokenType::Equal, "se esperaba '=' en asignación a arreglo");
        auto value = expression();
        consume(TokenType::Semicolon, "se esperaba ';' después de la asignación");
        return std::make_unique<ArrayAssignStmt>(name.lexeme, std::move(indices), std::move(value), loc);
    }
    auto expr = expression();
    consume(TokenType::Semicolon, "se esperaba ';' después de la expresión");
    return std::make_unique<ExprStmt>(std::move(expr), loc);
}

std::unique_ptr<Expr> Parser::expression() { return orExpr(); }

std::unique_ptr<Expr> Parser::orExpr() {
    auto expr = andExpr();
    while (match(TokenType::OrOr)) {
        Token op = previous();
        auto right = andExpr();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::andExpr() {
    auto expr = equality();
    while (match(TokenType::AndAnd)) {
        Token op = previous();
        auto right = equality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::equality() {
    auto expr = comparison();
    while (matchAny({TokenType::EqualEqual, TokenType::BangEqual})) {
        Token op = previous();
        auto right = comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::comparison() {
    auto expr = term();
    while (matchAny({TokenType::Greater, TokenType::GreaterEqual, TokenType::Less, TokenType::LessEqual})) {
        Token op = previous();
        auto right = term();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::term() {
    auto expr = factor();
    while (matchAny({TokenType::Plus, TokenType::Minus})) {
        Token op = previous();
        auto right = factor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::factor() {
    auto expr = unary();
    while (matchAny({TokenType::Star, TokenType::Slash, TokenType::Percent})) {
        Token op = previous();
        auto right = unary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op.lexeme, std::move(right), op.loc);
    }
    return expr;
}

std::unique_ptr<Expr> Parser::unary() {
    if (matchAny({TokenType::Bang, TokenType::Minus, TokenType::Ampersand, TokenType::Star})) {
        Token op = previous();
        auto right = unary();
        return std::make_unique<UnaryExpr>(op.lexeme, std::move(right), op.loc);
    }
    return call();
}

std::unique_ptr<Expr> Parser::call() {
    auto expr = primary();
    while (true) {
        if (match(TokenType::LeftParen)) {
            auto* id = dynamic_cast<IdExpr*>(expr.get());
            if (!id) throw SyntaxError(previous().loc, "solo se puede llamar a una función por nombre");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RightParen)) {
                do { args.push_back(expression()); } while (match(TokenType::Comma));
            }
            Token paren = consume(TokenType::RightParen, "se esperaba ')' después de argumentos");
            std::string callee = id->name;
            expr = std::make_unique<CallExpr>(callee, std::move(args), paren.loc);
        } else if (check(TokenType::Less) && current + 3 < tokens.size()
                   && tokens[current + 2].type == TokenType::Greater
                   && tokens[current + 3].type == TokenType::LeftParen) {
            advance(); // '<'
            auto* id = dynamic_cast<IdExpr*>(expr.get());
            if (!id) throw SyntaxError(previous().loc, "solo se puede especializar una función por nombre");
            std::vector<Type> typeArgs;
            typeArgs.push_back(parseTypeArgument());
            while (match(TokenType::Comma)) typeArgs.push_back(parseTypeArgument());
            consume(TokenType::Greater, "se esperaba '>' después de argumentos de tipo");
            consume(TokenType::LeftParen, "se esperaba '(' después de especialización de template");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RightParen)) {
                do { args.push_back(expression()); } while (match(TokenType::Comma));
            }
            Token paren = consume(TokenType::RightParen, "se esperaba ')' después de argumentos");
            expr = std::make_unique<CallExpr>(id->name, std::move(args), paren.loc, std::move(typeArgs));
        } else if (match(TokenType::LeftBracket)) {
            std::string arrayName;
            std::vector<std::unique_ptr<Expr>> indices;

            if (auto* id = dynamic_cast<IdExpr*>(expr.get())) {
                arrayName = id->name;
            } else if (auto* access = dynamic_cast<ArrayAccessExpr*>(expr.get())) {
                arrayName = access->name;
                indices = std::move(access->indices);
            } else {
                throw SyntaxError(previous().loc, "por ahora solo se soporta acceso a arreglo sobre identificadores");
            }

            indices.push_back(expression());
            Token close = consume(TokenType::RightBracket, "se esperaba ']' después del índice");
            expr = std::make_unique<ArrayAccessExpr>(arrayName, std::move(indices), close.loc);
        } else if (match(TokenType::Dot)) {
            auto* id = dynamic_cast<IdExpr*>(expr.get());
            if (!id) throw SyntaxError(previous().loc, "por ahora solo se soporta acceso obj.campo sobre identificadores");
            Token field = consume(TokenType::Identifier, "se esperaba nombre de campo después de '.'");
            expr = std::make_unique<FieldAccessExpr>(id->name, field.lexeme, field.loc);
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::primary() {
    if (match(TokenType::Number)) return std::make_unique<IntExpr>(std::stol(previous().lexeme), previous().loc);
    if (match(TokenType::True)) return std::make_unique<BoolExpr>(true, previous().loc);
    if (match(TokenType::False)) return std::make_unique<BoolExpr>(false, previous().loc);
    if (match(TokenType::CharLiteral)) return std::make_unique<CharExpr>(previous().lexeme.empty() ? '\0' : previous().lexeme[0], previous().loc);
    if (match(TokenType::StringLiteral)) return std::make_unique<StringExpr>(previous().lexeme, previous().loc);
    if (match(TokenType::New)) {
        SourceLocation loc = previous().loc;
        Type allocated = parseType();
        if (allocated.pointerDepth > 0 || allocated.isArray() || allocated.kind != TypeKind::Int) {
            throw SyntaxError(loc, "esta versión solo soporta new int");
        }
        return std::make_unique<NewExpr>(allocated, loc);
    }
    if (match(TokenType::Lambda)) {
        SourceLocation loc = previous().loc;
        Type ret = parseType();
        consume(TokenType::LeftParen, "se esperaba '(' después del tipo de retorno de lambda");
        auto params = parameters();
        consume(TokenType::RightParen, "se esperaba ')' después de parámetros de lambda");
        auto body = block();
        return std::make_unique<LambdaExpr>(ret, std::move(params), std::move(body), loc);
    }
    if (match(TokenType::Identifier)) return std::make_unique<IdExpr>(previous().lexeme, previous().loc);
    if (match(TokenType::LeftParen)) {
        auto expr = expression();
        consume(TokenType::RightParen, "se esperaba ')' después de expresión");
        return expr;
    }
    throw SyntaxError(peek().loc, "se esperaba expresión");
}


std::string Parser::traceName() {
    Token id = consume(TokenType::Identifier, "se esperaba identificador en trace");
    std::string name = id.lexeme;
    if (match(TokenType::Dot)) {
        Token field = consume(TokenType::Identifier, "se esperaba campo después de '.' en trace");
        name += "." + field.lexeme;
    }
    return name;
}

bool Parser::isFieldAssignmentStart() const {
    return check(TokenType::Identifier) && current + 3 < tokens.size()
        && tokens[current + 1].type == TokenType::Dot
        && tokens[current + 2].type == TokenType::Identifier
        && tokens[current + 3].type == TokenType::Equal;
}

bool Parser::isPointerAssignmentStart() const {
    return check(TokenType::Star) && current + 2 < tokens.size()
        && tokens[current + 1].type == TokenType::Identifier
        && tokens[current + 2].type == TokenType::Equal;
}

bool Parser::isArrayAssignmentStart() const {
    if (!(check(TokenType::Identifier) && current + 1 < tokens.size() && tokens[current + 1].type == TokenType::LeftBracket)) return false;

    size_t i = current + 1;
    while (i < tokens.size() && tokens[i].type == TokenType::LeftBracket) {
        i++;
        int depth = 1;
        while (i < tokens.size() && depth > 0) {
            if (tokens[i].type == TokenType::LeftBracket) depth++;
            else if (tokens[i].type == TokenType::RightBracket) depth--;
            i++;
        }
        if (depth != 0) return false;
    }
    return i < tokens.size() && tokens[i].type == TokenType::Equal;
}

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (TokenType t : types) if (check(t)) { advance(); return true; }
    return false;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return type == TokenType::EndOfFile;
    return peek().type == type;
}

Token Parser::advance() {
    if (!isAtEnd()) current++;
    return previous();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::EndOfFile;
}

Token Parser::peek() const { return tokens[current]; }
Token Parser::previous() const { return tokens[current - 1]; }

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw SyntaxError(peek().loc, message + ". Se recibió '" + peek().lexeme + "'");
}
