# Guía de demo para exposición

Esta guía propone un orden corto y claro para demostrar EduTrace.

## 1. Compilar el proyecto

```bash
make clean
make
```

## 2. Demostrar tests automáticos

```bash
make test
```

Resultado esperado:

```txt
Resumen: 78 OK, 0 FAIL
```

Explicar que esto valida lexer, parser, semántica, trazas, codegen y optimización.

## 3. Mostrar el objetivo educativo

Ejecutar:

```bash
./build/edutrace examples/while_loop.et --trace
```

Explicar cómo `trace loop` permite ver condiciones, iteraciones y cambios de variables.

## 4. Mostrar funciones y stack

```bash
./build/edutrace examples/functions.et --trace
```

O si se desea mostrar recursión:

```bash
./build/edutrace examples/recursion.et --trace
```

Explicar que esto ayuda a enseñar llamadas, retornos y pila.

## 5. Mostrar arrays

```bash
./build/edutrace examples/multidimensional_arrays.et --trace
```

Explicar `trace array` y cómo se visualizan filas y columnas.

## 6. Mostrar punteros y memoria dinámica

```bash
./build/edutrace examples/dynamic_memory.et --trace
```

Explicar `new int`, `*p`, `delete p` y `trace memory`.

## 7. Mostrar generación x86

```bash
./build/edutrace examples/variables.et --asm output.s
cat output.s
```

Luego ejecutar:

```bash
gcc -no-pie output.s -o output
./output
```

Explicar que el compilador produce ensamblador x86-64 funcional.

## 8. Mostrar optimización

```bash
./build/edutrace examples/optimization.et --opt-report
./build/edutrace examples/optimization.et --asm-no-opt no_opt.s
./build/edutrace examples/optimization.et --asm opt.s
wc -l no_opt.s opt.s
```

Explicar que `--asm` aplica optimización y `--asm-no-opt` no.

## 9. Mostrar benchmark

```bash
BENCH_REPS=1 make benchmark
```

Luego abrir:

```txt
benchmarks/results/optimization_effects.csv
```

Explicar casos como:

```txt
mixed_optimization: asm 76 -> 29
constant_folding: asm 46 -> 26
```

## 10. Cierre recomendado

Mensaje final sugerido:

```txt
EduTrace no intenta reemplazar a C, Python o Java. Su aporte es pedagógico:
permite escribir programas y visualizar su ejecución paso a paso, mientras el
compilador implementa fases reales como análisis léxico, sintáctico, semántico,
optimización y generación x86-64.
```
