#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "scanner.h"
#include "parser.h"
#include "typecheck_visitor.h"
#include "trace_visitor.h"
#include "optimizer_visitor.h"
#include "codegen_visitor.h"
#include "visitor.h"

class ASTPrinter : public Visitor {
public:
    explicit ASTPrinter(std::ostream& out) : out(out) {}

    void visit(Program* node) override {
        line("Program");
        level++;
        for (auto& st : node->structs) st->accept(*this);
        for (auto& fn : node->functions) fn->accept(*this);
        level--;
    }

    void visit(StructDecl* node) override {
        line("Struct " + node->name);
        level++;
        for (const auto& f : node->fields) line("Field " + f.name + ": " + f.type.toString());
        level--;
    }

    void visit(FunctionDecl* node) override {
        line("Function " + node->name + (node->isTemplate() ? "<" + node->templateParam + ">" : "") + " -> " + node->returnType.toString());
        level++;
        for (const auto& p : node->params) line("Param " + p.name + ": " + p.type.toString());
        node->body->accept(*this);
        level--;
    }

    void visit(BlockStmt* node) override {
        line("Block");
        level++;
        for (auto& stmt : node->statements) stmt->accept(*this);
        level--;
    }

    void visit(VarDeclStmt* node) override {
        line(std::string(node->inferred ? "LetDecl " : "VarDecl ") + node->name + ": " + node->type.toString());
        if (node->initializer) child(node->initializer.get());
    }
    void visit(AssignStmt* node) override { line("Assign " + node->name); child(node->value.get()); }
    void visit(ArrayAssignStmt* node) override {
        line("ArrayAssign " + node->name);
        for (auto& idx : node->indices) child(idx.get());
        child(node->value.get());
    }
    void visit(PointerAssignStmt* node) override { line("PointerAssign *" + node->pointerName); child(node->value.get()); }
    void visit(FieldAssignStmt* node) override { line("FieldAssign " + node->fullName()); child(node->value.get()); }
    void visit(DeleteStmt* node) override { line("Delete " + node->pointerName); }
    void visit(PrintStmt* node) override { line("Print"); child(node->expression.get()); }
    void visit(ReturnStmt* node) override { line("Return"); if (node->value) child(node->value.get()); }
    void visit(IfStmt* node) override { line("If"); child(node->condition.get()); node->thenBranch->accept(*this); if (node->elseBranch) node->elseBranch->accept(*this); }
    void visit(WhileStmt* node) override { line("While"); child(node->condition.get()); node->body->accept(*this); }
    void visit(TraceStmt* node) override { line("Trace " + traceKindName(node->kind)); }
    void visit(ExprStmt* node) override { line("ExprStmt"); child(node->expression.get()); }
    void visit(BinaryExpr* node) override { line("Binary '" + node->op + "'"); child(node->left.get()); child(node->right.get()); }
    void visit(UnaryExpr* node) override { line("Unary '" + node->op + "'"); child(node->right.get()); }
    void visit(IntExpr* node) override { line("Int " + std::to_string(node->value)); }
    void visit(BoolExpr* node) override { line(std::string("Bool ") + (node->value ? "true" : "false")); }
    void visit(CharExpr* node) override { line(std::string("Char '") + node->value + "'"); }
    void visit(StringExpr* node) override { line("String \"" + node->value + "\""); }
    void visit(IdExpr* node) override { line("Id " + node->name); }
    void visit(ArrayAccessExpr* node) override {
        line("ArrayAccess " + node->name);
        for (auto& idx : node->indices) child(idx.get());
    }
    void visit(FieldAccessExpr* node) override { line("FieldAccess " + node->fullName()); }
    void visit(NewExpr* node) override { line("New " + node->allocatedType.toString()); }
    void visit(LambdaExpr* node) override {
        line("Lambda -> " + node->returnType.toString());
        level++;
        for (const auto& p : node->params) line("Param " + p.name + ": " + p.type.toString());
        node->body->accept(*this);
        level--;
    }
    void visit(CallExpr* node) override {
        std::string name = node->callee;
        if (!node->typeArguments.empty()) {
            name += "<";
            for (size_t i = 0; i < node->typeArguments.size(); ++i) {
                if (i > 0) name += ", ";
                name += node->typeArguments[i].toString();
            }
            name += ">";
        }
        line("Call " + name);
        level++;
        for (auto& arg : node->arguments) arg->accept(*this);
        level--;
    }

private:
    std::ostream& out;
    int level = 0;
    void line(const std::string& text) { out << std::string(level * 2, ' ') << text << "\n"; }
    void child(Expr* expr) { level++; expr->accept(*this); level--; }
};

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw CompilerError("no se pudo abrir archivo: " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

static void printUsage() {
    std::cerr << "Uso:\n"
              << "  edutrace archivo.et --tokens\n"
              << "  edutrace archivo.et --ast\n"
              << "  edutrace archivo.et --check\n"
              << "  edutrace archivo.et --trace\n"
              << "  edutrace archivo.et --opt-ast\n"
              << "  edutrace archivo.et --opt-report\n"
              << "  edutrace archivo.et --asm salida.s\n"
              << "  edutrace archivo.et --asm-no-opt salida.s\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return 1;
    }

    std::string path = argv[1];
    std::string mode = argv[2];

    try {
        std::string source = readFile(path);
        Scanner scanner(source);
        auto tokens = scanner.scanTokens();

        if (mode == "--tokens") {
            for (const auto& t : tokens) {
                std::cout << tokenTypeName(t.type) << " '" << t.lexeme << "' (" << t.loc.toString() << ")\n";
            }
            return 0;
        }

        Parser parser(tokens);
        auto program = parser.parse();

        TypeCheckVisitor checker;
        checker.check(*program);

        if (mode == "--check") {
            std::cout << "Análisis léxico, sintáctico y semántico exitoso.\n";
            return 0;
        }

        if (mode == "--ast") {
            ASTPrinter printer(std::cout);
            program->accept(printer);
            return 0;
        }

        if (mode == "--trace") {
            TraceVisitor tracer(std::cout);
            tracer.run(*program);
            return 0;
        }

        if (mode == "--opt-ast") {
            OptimizerVisitor optimizer;
            optimizer.optimize(*program);
            ASTPrinter printer(std::cout);
            program->accept(printer);
            return 0;
        }

        if (mode == "--opt-report") {
            OptimizerVisitor optimizer;
            optimizer.optimize(*program);
            optimizer.printReport(std::cout);
            return 0;
        }

        if (mode == "--asm" || mode == "--asm-no-opt") {
            if (argc < 4) throw CompilerError("modo " + mode + " requiere ruta de salida .s");
            if (mode == "--asm") {
                OptimizerVisitor optimizer;
                optimizer.optimize(*program);
            }
            std::ofstream asmOut(argv[3]);
            if (!asmOut) throw CompilerError("no se pudo crear archivo asm: " + std::string(argv[3]));
            CodeGenVisitor codegen(asmOut);
            codegen.generate(*program);
            std::cout << "Código ensamblador generado en " << argv[3] << "\n";
            return 0;
        }

        throw CompilerError("modo desconocido: " + mode);
    } catch (const CompilerError& e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error interno: " << e.what() << "\n";
        return 1;
    }
}
