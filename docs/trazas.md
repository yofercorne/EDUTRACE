# Trazas educativas en EduTrace

## 1. Objetivo

El diferencial de EduTrace es permitir que un programa no solo se ejecute, sino que también explique su ejecución. Las directivas `trace` están diseñadas para ayudar a estudiantes y docentes a visualizar conceptos que normalmente se explican con dibujos, pizarras o múltiples `print` manuales.

## 2. Uso general

Las trazas se observan con:

```bash
./build/edutrace examples/traces.et --trace
```

Las directivas disponibles son:

```et
trace variables(x, y);
trace branch;
trace loop(i, suma);
trace stack;
trace recursion;
trace array(nums);
trace memory(x, p);
```

## 3. `trace variables`

Permite observar cambios de variables.

```et
fun int main() {
    var int x;
    var int y;

    x = 5;
    y = x + 3;

    trace variables(x, y);

    x = y * 2;
    print(x);
    return 0;
}
```

Salida representativa:

```txt
trace variables activado:
  x = 5
  y = 8
Variable x cambió: 5 -> 16
print: 16
```

Utilidad docente:

- Explicar asignación.
- Explicar evaluación de expresiones.
- Mostrar el estado del programa sin insertar múltiples `print`.

## 4. `trace branch`

Explica cómo se evalúa una condición y qué rama se ejecuta.

```et
trace branch;

if (edad >= 18) {
    print("mayor");
} else {
    print("menor");
}
```

Salida representativa:

```txt
Evaluando condición if:
  edad >= 18 => false
Se ejecuta la rama ELSE.
```

Utilidad docente:

- Explicar condiciones booleanas.
- Mostrar por qué se entra al `if` o al `else`.

## 5. `trace loop`

Explica iteraciones de un ciclo `while`.

```et
trace loop(i, suma);

while (i < 3) {
    suma = suma + i;
    i = i + 1;
}
```

Salida representativa:

```txt
Iteración 1:
  condición i < 3 => true
  i = 0
  suma = 0
Iteración 2:
  condición i < 3 => true
  i = 1
  suma = 0
Iteración 3:
  condición i < 3 => true
  i = 2
  suma = 1
Condición final i < 3 => false
Fin del ciclo.
```

Utilidad docente:

- Explicar acumuladores.
- Explicar contadores.
- Mostrar por qué un ciclo termina.

## 6. `trace stack`

Muestra llamadas a función, parámetros, retornos y pila de llamadas.

```et
fun int inc(int x) {
    return x + 1;
}

fun int main() {
    var int a;
    a = 4;

    trace stack;

    print(inc(a));
    return 0;
}
```

Salida representativa:

```txt
trace stack activado:
  función actual: main
  pila: main
Entrando a función: inc
pila de llamadas: main -> inc
parámetro x = 4
Retorna 5 desde inc
```

Utilidad docente:

- Explicar parámetros.
- Explicar retornos.
- Explicar llamadas anidadas.

## 7. `trace recursion`

Muestra llamadas recursivas de forma jerárquica.

```et
fun int factorial(int n) {
    trace recursion;

    if (n == 0) {
        return 1;
    }

    return n * factorial(n - 1);
}
```

Salida representativa:

```txt
factorial(3)
  factorial(2)
    factorial(1)
      factorial(0)
      retorna 1
    retorna 1
  retorna 2
retorna 6
```

Utilidad docente:

- Explicar pila de recursión.
- Visualizar caso base.
- Visualizar cómo se recomponen los retornos.

## 8. `trace array`

Muestra el contenido de arreglos.

### Arreglo 1D

```et
var int nums[3];
nums[0] = 10;
nums[1] = 20;
nums[2] = 30;

trace array(nums);
```

Salida:

```txt
Arreglo nums[3]
índice: 0 1 2
valor : 10 20 30
```

### Arreglo 2D

```et
var int mat[2][3];
mat[0][0] = 1;
mat[1][2] = 6;

trace array(mat);
```

Salida:

```txt
Arreglo mat[2][3]
fila 0: 1 0 0
fila 1: 0 0 6
```

Utilidad docente:

- Explicar índices.
- Explicar recorrido de arreglos.
- Explicar matrices como filas y columnas.

## 9. `trace memory`

Explica punteros y memoria dinámica.

```et
fun int main() {
    var int x;
    var int* p;

    x = 10;
    p = &x;

    trace memory(x, p);

    *p = 20;
    print(x);
    return 0;
}
```

Salida representativa:

```txt
trace memory activado:
  x = 10
  p = &x -> apunta a x = 10
Ejecutando *p = 20
  x cambió: 10 -> 20
```

Con memoria dinámica:

```et
var int* p;
p = new int;
*p = 50;
trace memory(p);
delete p;
```

Salida representativa:

```txt
Reservando memoria dinámica: heap#1 para int, valor inicial 0
p = &heap#1 -> apunta a heap#1 = 50
Liberando memoria dinámica apuntada por p
bloque heap#1 liberado
p = null
```

Utilidad docente:

- Explicar referencias.
- Explicar desreferencia.
- Explicar memoria dinámica.
- Reducir la dificultad visual de punteros.

## 10. Trace e inferencia con `let`

Cuando se usa `let`, EduTrace puede mostrar el tipo inferido.

```et
let x = 10;
let msg = "Hola";
let flag = x > 5;
```

Salida representativa:

```txt
Inferencia de tipo: x = 10 => int
Inferencia de tipo: msg = "Hola" => string
Inferencia de tipo: flag = x > 5 => bool
```

## 11. Trace con lambdas y templates

EduTrace integra lambdas y templates simples dentro de `trace stack`.

```et
let inc = lambda int(int x) {
    return x + 1;
};

trace stack;
print(inc(5));
```

Y también:

```et
fun T identity<T>(T x) {
    return x;
}

trace stack;
print(identity<int>(10));
```

Esto permite mostrar que las lambdas y templates se comportan como llamadas con parámetros y retorno.
