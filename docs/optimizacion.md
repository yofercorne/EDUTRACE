# Optimización en EduTrace

## 1. Objetivo

EduTrace implementa optimizaciones básicas sobre el AST antes de generar ensamblador x86-64. El objetivo no es competir con compiladores industriales, sino demostrar una fase de optimización funcional, medible y explicable.

La evidencia se obtiene comparando:

```txt
EduTrace-O0: generación sin optimización
EduTrace-O1: generación con optimización
```

## 2. Modos relacionados

```bash
./build/edutrace examples/optimization.et --opt-ast
./build/edutrace examples/optimization.et --opt-report
./build/edutrace examples/optimization.et --asm output_opt.s
./build/edutrace examples/optimization.et --asm-no-opt output_no_opt.s
```

Significado:

- `--opt-ast`: imprime el AST después de optimizar.
- `--opt-report`: muestra cuántas optimizaciones se aplicaron.
- `--asm`: genera ASM optimizado.
- `--asm-no-opt`: genera ASM sin optimización.

## 3. OptimizerVisitor

La optimización se implementa en:

```txt
src/optimizer_visitor.h
src/optimizer_visitor.cpp
```

El visitor recorre el AST y reemplaza nodos por versiones más simples cuando la transformación es segura.

## 4. Optimizaciones implementadas

### 4.1 Constant folding

Evalúa expresiones constantes durante la compilación.

Antes:

```et
x = 2 + 3 * 4;
```

Después:

```et
x = 14;
```

También aplica a comparaciones constantes:

```et
if (2 > 3) {
    print(1);
} else {
    print(0);
}
```

La condición se reduce a `false`.

### 4.2 Simplificación algebraica

Elimina operaciones neutras o redundantes.

Reglas implementadas:

```txt
x + 0 -> x
0 + x -> x
x - 0 -> x
x * 1 -> x
1 * x -> x
x * 0 -> 0
0 * x -> 0
x / 1 -> x
x % 1 -> 0
true && x -> x
false && x -> false
true || x -> true
false || x -> x
```

Ejemplo:

```et
x = (y + 0) * 1;
```

Se reduce a:

```et
x = y;
```

### 4.3 Dead code elimination

Elimina instrucciones después de un `return` dentro del mismo bloque.

Antes:

```et
fun int main() {
    print(1);
    return 0;
    print(999);
}
```

Después:

```et
fun int main() {
    print(1);
    return 0;
}
```

### 4.4 If con condición constante

Antes:

```et
if (true) {
    print(10);
} else {
    print(20);
}
```

Después:

```et
print(10);
```

Si la condición es `false`, se conserva solo el `else`.

### 4.5 Eliminación de `while(false)`

Antes:

```et
while (false) {
    print(999);
}
print(1);
```

Después:

```et
print(1);
```

## 5. Ejemplo mixto

```et
fun int main() {
    var int x;

    x = 2 + 3 * 4;

    if (true) {
        x = x + 0;
        print(x);
    } else {
        print(999);
    }

    return 0;
    print(123);
}
```

Optimizaciones aplicables:

```txt
2 + 3 * 4 -> 14
x + 0 -> x
if(true) -> conservar solo rama then
código después de return -> eliminado
```

## 6. Evidencia medible

La métrica principal usada es la reducción del ensamblador generado, manteniendo la misma salida.

Ejemplo de reporte esperado:

```txt
mixed_optimization: asm 76 -> 29, reducción 61.84%, salida igual
constant_folding:   asm 46 -> 26, reducción 43.48%, salida igual
while_false:        asm 44 -> 26, reducción 40.91%, salida igual
```

Esta métrica es más estable que el tiempo de ejecución, porque los tiempos pueden variar según el sistema operativo, carga de CPU y número de repeticiones.

## 7. Limitaciones actuales

EduTrace no implementa optimizaciones avanzadas como:

```txt
common subexpression elimination
inlining
loop unrolling
register allocation avanzado
propagación global de constantes
optimización interprocedural
```

La fase actual se enfoca en optimizaciones locales y seguras sobre AST.
