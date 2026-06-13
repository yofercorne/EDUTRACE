#!/usr/bin/env bash
set -u

BIN="${BIN:-./build/edutrace}"
TEST_BUILD="build/test_outputs"
PASS=0
FAIL=0

mkdir -p "$TEST_BUILD"

pass() {
    printf "[OK] %s\n" "$1"
    PASS=$((PASS + 1))
}

fail() {
    printf "[FAIL] %s\n" "$1"
    FAIL=$((FAIL + 1))
}

run_ok() {
    local name="$1"
    shift
    if "$@" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        pass "$name"
    else
        fail "$name"
        cat "$TEST_BUILD/tmp.err"
    fi
}

run_fail() {
    local name="$1"
    shift
    if "$@" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        fail "$name"
        echo "  Se esperaba error, pero el comando terminó correctamente."
    else
        pass "$name"
    fi
}

run_contains() {
    local name="$1"
    local expected="$2"
    shift 2
    if "$@" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err" && grep -q "$expected" "$TEST_BUILD/tmp.out"; then
        pass "$name"
    else
        fail "$name"
        echo "  No se encontró el texto esperado: $expected"
        echo "  STDOUT:"
        cat "$TEST_BUILD/tmp.out"
        echo "  STDERR:"
        cat "$TEST_BUILD/tmp.err"
    fi
}

run_not_contains() {
    local name="$1"
    local unexpected="$2"
    shift 2
    if "$@" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err" && ! grep -q "$unexpected" "$TEST_BUILD/tmp.out"; then
        pass "$name"
    else
        fail "$name"
        echo "  Se encontró texto no esperado: $unexpected"
        echo "  STDOUT:"
        cat "$TEST_BUILD/tmp.out"
        echo "  STDERR:"
        cat "$TEST_BUILD/tmp.err"
    fi
}

run_asm_smaller() {
    local name="$1"
    local source="$2"
    local base
    base=$(basename "$source" .et)
    local asm_no="$TEST_BUILD/${base}_no_opt.s"
    local asm_opt="$TEST_BUILD/${base}_opt.s"

    if ! "$BIN" "$source" --asm-no-opt "$asm_no" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        fail "$name: generación asm sin optimizar"
        cat "$TEST_BUILD/tmp.err"
        return
    fi
    if ! "$BIN" "$source" --asm "$asm_opt" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        fail "$name: generación asm optimizado"
        cat "$TEST_BUILD/tmp.err"
        return
    fi

    local no_count opt_count
    no_count=$(grep -v '^\.' "$asm_no" | grep -v '^$' | wc -l)
    opt_count=$(grep -v '^\.' "$asm_opt" | grep -v '^$' | wc -l)
    if [[ "$opt_count" -lt "$no_count" ]]; then
        pass "$name"
    else
        fail "$name"
        echo "  Instrucciones sin optimizar: $no_count"
        echo "  Instrucciones optimizadas:   $opt_count"
    fi
}

run_codegen() {
    local name="$1"
    local source="$2"
    local expected="$3"
    local base
    base=$(basename "$source" .et)
    local asm="$TEST_BUILD/${base}.s"
    local exe="$TEST_BUILD/${base}.out"
    local out="$TEST_BUILD/${base}.stdout"

    if ! "$BIN" "$source" --asm "$asm" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        fail "$name: generación asm"
        cat "$TEST_BUILD/tmp.err"
        return
    fi

    if ! gcc -no-pie "$asm" -o "$exe" > "$TEST_BUILD/tmp.out" 2> "$TEST_BUILD/tmp.err"; then
        fail "$name: ensamblado gcc"
        cat "$TEST_BUILD/tmp.err"
        return
    fi

    "$exe" > "$out"
    local actual
    actual=$(tr -d '\r' < "$out" | sed 's/[[:space:]]*$//')
    if [[ "$actual" == "$expected" ]]; then
        pass "$name"
    else
        fail "$name"
        echo "  Esperado: $expected"
        echo "  Recibido: $actual"
    fi
}

if [[ ! -x "$BIN" ]]; then
    echo "No se encontró el binario $BIN. Ejecuta: make"
    exit 1
fi

echo "== Lexer =="
run_ok "lexer acepta tokens válidos" "$BIN" tests/lexer/valid_tokens.et --tokens

echo

echo "== Parser =="
run_ok "parser acepta sintaxis válida" "$BIN" tests/parser/valid_syntax.et --ast
run_fail "parser detecta punto y coma faltante" "$BIN" tests/parser/missing_semicolon.et --check

echo

echo "== Semántica =="
run_ok "typechecker acepta programa válido" "$BIN" tests/semantic/valid_basic.et --check
run_fail "typechecker detecta asignación inválida" "$BIN" tests/semantic/invalid_type_assignment.et --check
run_fail "typechecker detecta variable no declarada" "$BIN" tests/semantic/undefined_variable.et --check
run_fail "typechecker detecta retorno inválido" "$BIN" tests/semantic/invalid_return.et --check
run_fail "typechecker detecta condición no booleana" "$BIN" tests/semantic/invalid_condition.et --check
run_fail "typechecker detecta trace de variable inexistente" "$BIN" tests/semantic/invalid_trace_variable.et --check
run_ok "typechecker acepta arreglos" "$BIN" tests/semantic/valid_array.et --check
run_fail "typechecker detecta índice no entero" "$BIN" tests/semantic/invalid_array_index_type.et --check
run_fail "typechecker detecta índice fuera de rango" "$BIN" tests/semantic/invalid_array_out_of_range.et --check
run_fail "typechecker detecta índice fuera de rango en matriz" "$BIN" tests/semantic/invalid_multidimensional_array_out_of_range.et --check
run_fail "typechecker detecta demasiados índices en matriz" "$BIN" tests/semantic/invalid_multidimensional_array_too_many_indices.et --check
run_fail "typechecker detecta trace array inválido" "$BIN" tests/semantic/invalid_trace_array_non_array.et --check
run_ok "typechecker acepta strings" "$BIN" tests/semantic/valid_string.et --check
run_fail "typechecker detecta asignación inválida a string" "$BIN" tests/semantic/invalid_string_assignment.et --check
run_ok "typechecker acepta structs" "$BIN" tests/semantic/valid_struct.et --check
run_fail "typechecker detecta campo inexistente" "$BIN" tests/semantic/invalid_struct_field.et --check
run_fail "typechecker detecta tipo inválido en campo" "$BIN" tests/semantic/invalid_struct_field_type.et --check
run_fail "typechecker detecta struct no declarado" "$BIN" tests/semantic/unknown_struct_type.et --check
run_ok "typechecker acepta punteros" "$BIN" tests/semantic/valid_pointer.et --check
run_ok "typechecker acepta memoria dinámica" "$BIN" tests/semantic/valid_dynamic_memory.et --check
run_ok "typechecker acepta let con inferencia" "$BIN" tests/semantic/valid_let_inference.et --check
run_ok "typechecker acepta char y promoción char a int" "$BIN" tests/semantic/valid_char_promotion.et --check
run_ok "typechecker acepta arreglos multidimensionales" "$BIN" tests/semantic/valid_multidimensional_array.et --check
run_ok "typechecker acepta lambdas simples" "$BIN" tests/semantic/valid_lambda.et --check
run_ok "typechecker acepta templates simples" "$BIN" tests/semantic/valid_template.et --check
run_fail "typechecker detecta template sin argumento de tipo" "$BIN" tests/semantic/invalid_template_missing_type_arg.et --check
run_fail "typechecker detecta argumento incompatible en template" "$BIN" tests/semantic/invalid_template_argument_type.et --check
run_fail "typechecker detecta retorno inválido en template" "$BIN" tests/semantic/invalid_template_return.et --check
run_fail "parser detecta let sin inicializador" "$BIN" tests/semantic/invalid_let_without_initializer.et --check
run_fail "typechecker detecta asignación inválida a let string" "$BIN" tests/semantic/invalid_let_assignment_type.et --check
run_fail "typechecker detecta asignación int a char" "$BIN" tests/semantic/invalid_char_assignment.et --check
run_fail "typechecker detecta operación char con string" "$BIN" tests/semantic/invalid_char_string_operation.et --check
run_fail "typechecker detecta desreferencia no puntero" "$BIN" tests/semantic/invalid_deref_non_pointer.et --check
run_fail "typechecker detecta asignación incompatible por puntero" "$BIN" tests/semantic/invalid_pointer_assignment_type.et --check
run_fail "typechecker detecta asignación mediante no puntero" "$BIN" tests/semantic/invalid_pointer_assign_non_pointer.et --check
run_fail "typechecker detecta delete sobre no puntero" "$BIN" tests/semantic/invalid_delete_non_pointer.et --check
run_fail "typechecker detecta retorno inválido en lambda" "$BIN" tests/semantic/invalid_lambda_return.et --check
run_fail "typechecker detecta aridad inválida en lambda" "$BIN" tests/semantic/invalid_lambda_arity.et --check
run_fail "typechecker detecta captura no permitida en lambda" "$BIN" tests/semantic/invalid_lambda_capture.et --check

echo

echo "== Trace educativo =="
run_contains "trace variables muestra cambio de x" "Variable x cambió" "$BIN" tests/trace/trace_variables.et --trace
run_contains "trace loop muestra iteraciones" "Iteración 1 del while" "$BIN" tests/trace/trace_loop.et --trace
run_contains "trace loop muestra condición" "Evaluando condición while" "$BIN" tests/trace/trace_loop_condition_only.et --trace
run_contains "trace branch muestra rama else" "Se ejecuta la rama ELSE" "$BIN" tests/trace/trace_branch.et --trace
run_contains "trace stack muestra llamada a función" "Entrando a función: inc" "$BIN" tests/trace/trace_stack.et --trace
run_contains "trace array muestra arreglo" "Arreglo nums" "$BIN" tests/trace/trace_array.et --trace
run_contains "trace array muestra matriz" "fila 1" "$BIN" tests/trace/trace_multidimensional_array.et --trace
run_contains "trace variables muestra string" "Variable msg cambió" "$BIN" tests/trace/trace_string.et --trace
run_contains "trace variables muestra campo de struct" "Variable s.age cambió" "$BIN" tests/trace/trace_struct.et --trace
run_contains "trace memory muestra puntero" "apunta a x" "$BIN" tests/trace/trace_memory.et --trace
run_contains "trace memory muestra memoria dinámica" "Reservando memoria dinámica" "$BIN" tests/trace/trace_dynamic_memory.et --trace
run_contains "trace muestra inferencia de let" "Inferencia de tipo: y" "$BIN" tests/trace/trace_let_inference.et --trace
run_contains "trace muestra char" "Variable c cambió" "$BIN" tests/trace/trace_char_promotion.et --trace
run_contains "trace stack muestra entrada a lambda" "Entrando a lambda" "$BIN" tests/trace/trace_lambda.et --trace
run_contains "trace stack muestra entrada a función template" "Entrando a función: identity" "$BIN" tests/trace/trace_template.et --trace

echo

echo "== Codegen x86 =="
run_codegen "codegen aritmética básica" tests/codegen/basic_arithmetic.et "14"
run_codegen "codegen if/else" tests/codegen/if_else_codegen.et "2"
run_codegen "codegen while" tests/codegen/while_codegen.et "3"
run_codegen "codegen arreglos" tests/codegen/array_codegen.et "10"
run_codegen "codegen matrices 2D" tests/codegen/multidimensional_array_codegen.et "6"
run_codegen "codegen strings" tests/codegen/string_codegen.et "Hola EduTrace"
run_codegen "codegen structs" tests/codegen/struct_codegen.et $'Ana\n18'
run_codegen "codegen punteros" tests/codegen/pointer_codegen.et $'20\n20'
run_codegen "codegen memoria dinámica" tests/codegen/dynamic_memory_codegen.et "50"
run_codegen "codegen let con inferencia" tests/codegen/let_codegen.et $'15
Hola let'
run_codegen "codegen char y promoción" tests/codegen/char_codegen.et $'A
66'
run_codegen "codegen lambda simple" tests/codegen/lambda_codegen.et "6"
run_codegen "codegen template simple" tests/codegen/template_codegen.et $'10
template
Z'


echo

echo "== Optimización =="
run_contains "optimizer aplica constant folding" "Int 14" "$BIN" tests/optimization/constant_folding.et --opt-ast
run_contains "optimizer reporta constant folding" "Constant folding: 2" "$BIN" tests/optimization/constant_folding.et --opt-report
run_contains "optimizer reporta simplificación algebraica" "Simplificaciones algebraicas: 2" "$BIN" tests/optimization/algebraic_simplification.et --opt-report
run_contains "optimizer elimina código muerto" "Código muerto eliminado: 2" "$BIN" tests/optimization/dead_code.et --opt-report
run_contains "optimizer simplifica if constante" "If con condición constante simplificados: 1" "$BIN" tests/optimization/constant_if.et --opt-report
run_not_contains "optimizer elimina nodo If constante" "If" "$BIN" tests/optimization/constant_if.et --opt-ast
run_contains "optimizer elimina while(false)" "While(false) eliminados: 1" "$BIN" tests/optimization/constant_while_false.et --opt-report
run_asm_smaller "optimizer reduce ensamblador generado" tests/optimization/optimized_codegen.et

echo
printf "Resumen: %d OK, %d FAIL\n" "$PASS" "$FAIL"

if [[ "$FAIL" -ne 0 ]]; then
    exit 1
fi
