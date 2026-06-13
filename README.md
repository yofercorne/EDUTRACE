# EduTrace

EduTrace es un lenguaje de programación educativo, imperativo, estáticamente tipado y compilado a x86-64. Su objetivo principal es apoyar la enseñanza de programación mediante trazas de ejecución que explican cómo cambian las variables, cómo se evalúan condicionales, cómo funcionan los ciclos, cómo se comportan las llamadas a función, cómo se visualizan arreglos y cómo se entiende el uso de punteros y memoria dinámica.

El proyecto implementa un compilador completo con análisis léxico, análisis sintáctico, AST, análisis semántico, verificación de tipos, optimización básica, generación de ensamblador x86-64, tests automáticos y benchmarks comparativos.

## Diferenciador del lenguaje

EduTrace no busca reemplazar lenguajes como C, Python o Java. Su propósito es ser útil en contextos de enseñanza. Por eso, además de ejecutar programas, permite escribir directivas educativas como:

```et
trace variables(x, y);
trace branch;
trace loop(i, suma);
trace stack;
trace recursion;
trace array(nums);
trace memory(x, p);
```

Estas directivas permiten generar salidas explicativas sin llenar el programa con `print` manuales.

## Estructura del proyecto

```txt
EduTrace/
├── src/                  # Código fuente del compilador
├── examples/             # Programas de ejemplo en EduTrace
├── tests/                # Pruebas automáticas
├── benchmarks/           # Benchmarks EduTrace vs C/GCC
└── docs/                 # Documentación técnica
```

## Compilación

Desde la raíz del proyecto:

```bash
make
```

Esto genera el ejecutable:

```bash
./build/edutrace
```

## Modos de uso

```bash
./build/edutrace archivo.et --tokens
./build/edutrace archivo.et --ast
./build/edutrace archivo.et --check
./build/edutrace archivo.et --trace
./build/edutrace archivo.et --opt-ast
./build/edutrace archivo.et --opt-report
./build/edutrace archivo.et --asm salida.s
./build/edutrace archivo.et --asm-no-opt salida.s
```

### Ejemplo de flujo completo

```bash
make
./build/edutrace examples/variables.et --tokens
./build/edutrace examples/variables.et --ast
./build/edutrace examples/variables.et --check
./build/edutrace examples/variables.et --trace
./build/edutrace examples/variables.et --asm output.s
gcc -no-pie output.s -o output
./output
```

## Tests

```bash
make clean
make test
```

Resultado esperado de esta versión:

```txt
Resumen: 78 OK, 0 FAIL
```

## Benchmarks

```bash
make benchmark
```

Opcionalmente, controlar repeticiones:

```bash
BENCH_REPS=5 make benchmark
```

Los resultados se generan en:

```txt
benchmarks/results/benchmark_results.csv
benchmarks/results/optimization_effects.csv
```

## Características soportadas

### Básicas

- Tipos `int`, `bool`, `char`, `string` y `void`.
- Variables con `var`.
- Inferencia de tipos con `let`.
- Scope por bloques y funciones.
- Funciones y recursión.
- `if / else`.
- `while`.
- `struct`.
- Arreglos unidimensionales.
- Arreglos bidimensionales.
- Cadenas de caracteres mediante `string`.
- Impresión con `print`.

### Avanzadas

- Punteros básicos: `int*`, `&x`, `*p`, `*p = valor`.
- Memoria dinámica básica: `new int` y `delete p`.
- Promoción automática `char -> int`.
- Lambdas simples sin captura.
- Templates simples de función con un parámetro genérico `T`.
- Optimización básica sobre AST.

### Educativas

- `trace variables`.
- `trace branch`.
- `trace loop`.
- `trace stack`.
- `trace recursion`.
- `trace array`.
- `trace memory`.

## Ejemplo representativo

```et
struct Student {
    int age;
    string name;
}

fun T identity<T>(T x) {
    return x;
}

fun int main() {
    var Student s;
    var int nums[3];
    var int* p;

    s.age = 18;
    s.name = "Ana";

    nums[0] = 10;
    nums[1] = 20;
    nums[2] = nums[0] + nums[1];

    p = new int;
    *p = nums[2];

    trace variables(s.name, s.age);
    trace array(nums);
    trace memory(p);

    print(identity<string>(s.name));
    print(*p);

    delete p;
    return 0;
}
```

## Documentación técnica

- `docs/lenguaje.md`: definición del lenguaje.
- `docs/arquitectura.md`: arquitectura del compilador.
- `docs/trazas.md`: directivas educativas.
- `docs/optimizacion.md`: optimizaciones implementadas.
- `docs/benchmarks.md`: evaluación experimental.
- `docs/reporte_tecnico.md`: resumen técnico para informe.
- `docs/guia_demo.md`: guía sugerida para exposición.

## Aplicación web simple

La versión actual incluye una aplicación funcional mínima en `app/` para demostrar el compilador desde navegador.

Ejecutar:

```bash
make app
```

Abrir:

```txt
http://localhost:8000
```

La app permite escribir código EduTrace y ejecutar:

```txt
check, tokens, AST, trace, reporte de optimización, generación ASM y ejecución real.
```

Esta sección cumple el objetivo de aplicación integrada mencionado en el enunciado: editor de código, visualización de AST, generación x86, ejecución/simulación y visualización de resultados.
