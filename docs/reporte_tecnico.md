# Reporte técnico de EduTrace

Este documento resume los puntos principales que deberían aparecer en el informe final del proyecto.

## 1. Título

**EduTrace: lenguaje educativo compilado a x86-64 para enseñar programación mediante trazas de ejecución**

## 2. Problema identificado

En cursos introductorios de programación, muchos estudiantes logran ejecutar código, pero no siempre comprenden qué ocurre internamente durante la ejecución. Conceptos como variables, ciclos, condicionales, llamadas a función, recursión, arreglos, punteros y memoria dinámica suelen requerir dibujos o explicaciones adicionales del docente.

EduTrace busca resolver este problema mediante un lenguaje que permite instrumentar programas con directivas educativas de trazado.

## 3. Objetivo del proyecto

Diseñar e implementar un compilador funcional para EduTrace, un lenguaje educativo que permite escribir programas imperativos y generar trazas explicativas de ejecución. El compilador incluye análisis léxico, sintáctico y semántico, verificación de tipos, optimización básica, generación x86-64 y evaluación comparativa mediante benchmarks.

## 4. Diferenciador

El principal diferenciador es la instrucción `trace`, que permite observar la ejecución de forma didáctica.

Ejemplos:

```et
trace variables(x, y);
trace branch;
trace loop(i, suma);
trace stack;
trace recursion;
trace array(nums);
trace memory(x, p);
```

Estas directivas permiten que el estudiante vea no solo el resultado final, sino también el proceso que lleva a ese resultado.

## 5. Características implementadas

### Básicas

- Tipos `int`, `bool`, `char`, `string`, `void`.
- Variables y scope.
- Funciones y recursión.
- `if / else`.
- `while`.
- Structs.
- Arreglos 1D y 2D.
- Strings.

### Avanzadas

- Punteros básicos.
- Memoria dinámica con `new int` y `delete p`.
- Templates simples de función.
- Inferencia con `let`.
- Promoción `char -> int`.
- Lambdas simples sin captura.
- Arreglos multidimensionales 2D.

### Compilador

- Lexer.
- Parser.
- AST.
- Tabla de símbolos.
- Typechecker.
- OptimizerVisitor.
- TraceVisitor.
- CodeGenVisitor.
- Generación x86-64.
- Tests automáticos.
- Benchmarks.

## 6. Arquitectura

```txt
source.et
  -> Scanner
  -> Parser
  -> AST
  -> TypeCheckVisitor
  -> OptimizerVisitor
  -> CodeGenVisitor
  -> asm x86-64
```

Modo educativo:

```txt
source.et
  -> Scanner
  -> Parser
  -> AST
  -> TypeCheckVisitor
  -> TraceVisitor
  -> traza educativa
```

## 7. Decisiones de diseño

### Uso del patrón Visitor

Se usa Visitor porque el AST debe recorrerse para distintas fases:

```txt
TypeCheckVisitor
TraceVisitor
OptimizerVisitor
CodeGenVisitor
```

Esto evita tener un único archivo con toda la lógica mezclada.

### Separación de modo normal y modo educativo

EduTrace puede generar ensamblador y ejecutar programas como un lenguaje compilado, pero también puede ejecutar un modo de traza para fines educativos.

### Templates y lambdas acotadas

Se implementan versiones simples para cumplir el alcance avanzado sin convertir el proyecto en una implementación completa de C++.

## 8. Optimización

Optimizaciones implementadas:

```txt
constant folding
simplificación algebraica
eliminación de código muerto
if con condición constante
while(false)
```

La evidencia se obtiene con:

```bash
make benchmark
```

Y se revisa en:

```txt
benchmarks/results/optimization_effects.csv
```

## 9. Benchmarks

Se comparan:

```txt
EduTrace-O0
EduTrace-O1
GCC-O0
GCC-O1
```

Se evalúan métricas como:

```txt
tiempo de compilación
tiempo de ejecución
salida generada
líneas ASM
reducción porcentual de ASM
```

## 10. Resultados esperados de pruebas

```bash
make test
```

Resultado esperado:

```txt
Resumen: 78 OK, 0 FAIL
```

## 11. Limitaciones actuales

- No hay `for` todavía.
- No hay `new int[10]`.
- No hay `new Struct`.
- No hay structs genéricos.
- No hay templates con múltiples parámetros.
- No hay closures reales en lambdas.
- La optimización es local sobre AST, no es un optimizador industrial.

## 12. Conclusión técnica

EduTrace cumple con las fases principales de un compilador y agrega un diferencial pedagógico. Su valor no está en competir con lenguajes industriales, sino en ayudar a estudiantes y docentes a visualizar cómo se ejecuta un programa. La implementación demuestra análisis léxico, sintáctico y semántico, generación x86-64, optimización básica y evaluación experimental con benchmarks.
