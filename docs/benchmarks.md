# Benchmarks y comparación experimental

## 1. Objetivo

Los benchmarks buscan demostrar tres aspectos:

1. EduTrace genera programas correctos.
2. EduTrace-O1 aplica optimizaciones medibles respecto a EduTrace-O0.
3. Los programas pueden compararse con versiones equivalentes en C compiladas con GCC.

El objetivo no es demostrar que EduTrace supera a GCC, sino presentar una evaluación experimental ordenada y defendible.

## 2. Herramientas comparadas

```txt
EduTrace-O0: EduTrace sin optimización
EduTrace-O1: EduTrace con optimización AST
GCC-O0: programa C equivalente compilado con gcc -O0
GCC-O1: programa C equivalente compilado con gcc -O1
```

## 3. Ejecución

Desde la raíz del proyecto:

```bash
make benchmark
```

Con cantidad de repeticiones configurable:

```bash
BENCH_REPS=5 make benchmark
```

## 4. Programas evaluados

### Benchmarks generales

```txt
factorial_iterativo
factorial_recursivo
fibonacci
```

Estos sirven como casos de control. No siempre muestran reducción fuerte de ASM porque dependen de valores dinámicos, ciclos y llamadas recursivas.

### Benchmarks específicos de optimización

```txt
constantes
constant_folding
algebraic_simplification
dead_code
if_constant
while_false
mixed_optimization
```

Estos están diseñados para evidenciar optimizaciones concretas.

## 5. Archivos de entrada

Versiones EduTrace:

```txt
benchmarks/edutrace/*.et
```

Versiones C equivalentes:

```txt
benchmarks/c_equivalent/*.c
```

## 6. Archivos de salida

El script genera resultados en:

```txt
benchmarks/results/
```

### `benchmark_results.csv`

Incluye todos los resultados por herramienta.

Columnas:

```txt
benchmark
category
tool
compile_ms
run_ms_avg
output
asm_lines
opt_report
```

### `optimization_effects.csv`

Es el archivo más importante para defender la optimización. Compara directamente EduTrace-O0 contra EduTrace-O1.

Columnas:

```txt
benchmark
optimization_focus
output_o0
output_o1
same_output
asm_o0
asm_o1
asm_reduction_lines
asm_reduction_pct
run_o0_ms
run_o1_ms
run_speedup_pct
opt_report
```

## 7. Interpretación de resultados

Una optimización se considera correctamente demostrada cuando:

```txt
same_output = yes
asm_o1 < asm_o0
asm_reduction_pct > 0
```

Ejemplo:

```txt
mixed_optimization: asm 76 -> 29, reducción 61.84%, salida yes
```

Esto significa que:

- La salida del programa no cambió.
- El código generado se redujo.
- La optimización fue semánticamente segura para ese caso.

## 8. Por qué no siempre mejora factorial o fibonacci

En programas como factorial o fibonacci, el resultado depende de valores en tiempo de ejecución. Las optimizaciones actuales de EduTrace son principalmente locales y sobre AST, por lo que no siempre pueden simplificar ciclos o recursión dinámica.

Esto no indica un fallo del optimizador. Indica que el impacto depende del tipo de programa.

## 9. Métricas usadas

```txt
compile_ms: tiempo de compilación o generación
run_ms_avg: tiempo promedio de ejecución
output: salida del programa
asm_lines: líneas no vacías de ASM generado
asm_reduction_pct: reducción porcentual de ASM entre O0 y O1
```

La métrica más estable para la defensa es `asm_reduction_pct`, porque el tiempo de ejecución puede fluctuar.

## 10. Uso recomendado en el reporte

Incluir una tabla con:

```txt
Benchmark | Optimización | ASM O0 | ASM O1 | Reducción | Salida igual
```

Ejemplo:

```txt
constant_folding | constant folding | 46 | 26 | 43.48% | yes
mixed_optimization | mixto | 76 | 29 | 61.84% | yes
```

Luego explicar que los benchmarks generales se usan para comprobar funcionamiento y comparación con GCC, mientras que los benchmarks específicos se usan para demostrar optimización.
