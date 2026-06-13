# Arquitectura del compilador EduTrace

## 1. Visión general

EduTrace se implementa como un compilador en C++17. La arquitectura separa las fases principales del proceso de compilación y usa el patrón Visitor para recorrer el AST con distintos propósitos.

```txt
Código fuente .et
    |
    v
Scanner / Lexer
    |
    v
Parser
    |
    v
AST
    |
    +--> TypeCheckVisitor
    |
    +--> TraceVisitor
    |
    +--> OptimizerVisitor
    |
    +--> CodeGenVisitor
    v
ASM x86-64
```

## 2. Organización de carpetas

```txt
src/
├── main.cpp
├── scanner.h / scanner.cpp
├── parser.h / parser.cpp
├── ast.h / ast.cpp
├── visitor.h
├── typecheck_visitor.h / .cpp
├── trace_visitor.h / .cpp
├── optimizer_visitor.h / .cpp
├── codegen_visitor.h / .cpp
├── symbol_table.h / .cpp
├── types.h
└── errors.h
```

## 3. Fase léxica

Archivos:

```txt
scanner.h
scanner.cpp
```

Responsabilidades:

- Leer caracteres del archivo fuente.
- Reconocer tokens.
- Detectar errores léxicos.
- Registrar ubicación mediante línea y columna.

Ejemplo:

```et
trace variables(x, y);
```

Produce tokens conceptuales:

```txt
TRACE VARIABLES LPAREN IDENTIFIER COMMA IDENTIFIER RPAREN SEMICOLON
```

## 4. Fase sintáctica

Archivos:

```txt
parser.h
parser.cpp
```

Responsabilidades:

- Consumir tokens.
- Validar estructura sintáctica.
- Construir el AST.
- Detectar errores sintácticos.

Ejemplo:

```et
x = 2 + 3;
```

Se transforma en:

```txt
AssignStmt
├── name: x
└── BinaryExpr(+)
    ├── IntExpr(2)
    └── IntExpr(3)
```

## 5. AST

Archivos:

```txt
ast.h
ast.cpp
```

El AST representa la estructura del programa. Algunos nodos relevantes son:

```txt
Program
StructDecl
FunctionDecl
BlockStmt
VarDeclStmt
AssignStmt
ArrayAssignStmt
FieldAssignStmt
PointerAssignStmt
IfStmt
WhileStmt
ReturnStmt
PrintStmt
TraceStmt
BinaryExpr
UnaryExpr
CallExpr
ArrayAccessExpr
FieldAccessExpr
LambdaExpr
NewExpr
```

## 6. Patrón Visitor

Archivo base:

```txt
visitor.h
```

Visitors principales:

```txt
TypeCheckVisitor
TraceVisitor
OptimizerVisitor
CodeGenVisitor
```

La ventaja es que el AST no mezcla responsabilidades. El mismo árbol puede ser recorrido para validar tipos, generar trazas, optimizar o generar ensamblador.

## 7. TypeCheckVisitor

Archivos:

```txt
typecheck_visitor.h
typecheck_visitor.cpp
```

Responsabilidades:

- Verificar variables declaradas.
- Verificar scopes.
- Validar asignaciones.
- Validar retornos de funciones.
- Validar llamadas a funciones.
- Validar arrays, structs, punteros, lambdas y templates simples.
- Validar uso correcto de directivas `trace`.

Ejemplos de errores detectados:

```txt
variable no declarada
tipo incompatible en asignación
return inválido
índice fuera de rango constante
campo inexistente en struct
delete sobre no puntero
lambda con captura no permitida
llamada template sin argumento de tipo
```

## 8. TraceVisitor

Archivos:

```txt
trace_visitor.h
trace_visitor.cpp
```

Responsabilidades:

- Simular la ejecución del programa.
- Mantener un entorno de variables.
- Mantener pila de llamadas.
- Mostrar trazas educativas.
- Explicar cambios de variables, ramas, ciclos, llamadas, arreglos y memoria.

Este visitor implementa el diferencial principal del lenguaje.

## 9. OptimizerVisitor

Archivos:

```txt
optimizer_visitor.h
optimizer_visitor.cpp
```

Responsabilidades:

- Recorrer y transformar el AST.
- Aplicar optimizaciones seguras.
- Registrar un reporte cuantitativo.

Optimizaciones:

```txt
constant folding
simplificación algebraica
eliminación de código muerto
simplificación de if con condición constante
eliminación de while(false)
```

## 10. CodeGenVisitor

Archivos:

```txt
codegen_visitor.h
codegen_visitor.cpp
```

Responsabilidades:

- Generar ensamblador x86-64.
- Crear etiquetas para saltos.
- Manejar stack frame de funciones.
- Generar llamadas a funciones.
- Generar código para impresión.
- Generar código para arrays, structs, punteros, lambdas y templates simples.
- Usar `malloc` y `free` para `new int` y `delete p`.

## 11. Tabla de símbolos

Archivos:

```txt
symbol_table.h
symbol_table.cpp
```

Responsabilidades:

- Registrar variables.
- Registrar funciones.
- Registrar structs.
- Manejar scopes.
- Guardar tipos y posiciones relativas necesarias para codegen.

## 12. Tipos

Archivo:

```txt
types.h
```

Representa tipos básicos y compuestos:

```txt
int
bool
char
string
void
puntero
array
struct
lambda
generic
```

## 13. Flujo por modo de ejecución

### `--check`

```txt
Scanner -> Parser -> AST -> TypeCheckVisitor
```

### `--trace`

```txt
Scanner -> Parser -> AST -> TypeCheckVisitor -> TraceVisitor
```

### `--asm`

```txt
Scanner -> Parser -> AST -> TypeCheckVisitor -> OptimizerVisitor -> CodeGenVisitor
```

### `--asm-no-opt`

```txt
Scanner -> Parser -> AST -> TypeCheckVisitor -> CodeGenVisitor
```

### `--opt-report`

```txt
Scanner -> Parser -> AST -> TypeCheckVisitor -> OptimizerVisitor -> reporte
```

## 14. Decisión de diseño: múltiples visitors

Durante el curso se puede trabajar con un único visitor grande. En EduTrace se decidió separar visitors porque el proyecto tiene varias responsabilidades:

```txt
typechecking
tracing educativo
optimización
generación x86
```

Separarlos evita mezclar lógica y facilita la documentación, las pruebas y la exposición técnica.
