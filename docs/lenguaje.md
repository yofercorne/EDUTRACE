# Lenguaje EduTrace

## 1. Propósito

EduTrace es un lenguaje imperativo, estáticamente tipado y compilado a x86-64. Está diseñado para apoyar la enseñanza de programación mediante trazas educativas. El objetivo no es competir con lenguajes industriales, sino ofrecer una herramienta clara para explicar conceptos de programación paso a paso.

## 2. Estructura general de un programa

Un programa está compuesto por declaraciones de structs y funciones. La función principal debe llamarse `main`.

```et
fun int main() {
    print(10);
    return 0;
}
```

## 3. Tipos

### Tipos básicos

```txt
int
bool
char
string
void
```

### Tipos compuestos

```txt
int*              puntero a int
int[5]            arreglo de int de tamaño 5
int[2][3]         matriz 2D de 2 filas y 3 columnas
Student           tipo definido por struct
lambda int(int)   lambda simple sin captura
T                 parámetro genérico en funciones template simples
```

## 4. Variables

### Declaración explícita

```et
var int x;
var bool flag;
var string msg;
var char c;
```

### Inferencia con `let`

`let` deduce el tipo a partir del inicializador.

```et
let x = 10;          // int
let msg = "Hola";   // string
let flag = x > 5;    // bool
let p = new int;     // int*
```

Reglas:

- `let` requiere inicializador.
- El tipo inferido queda fijo.
- No se permite cambiar luego a un tipo incompatible.

## 5. Operadores

### Aritméticos

```txt
+  -  *  /  %
```

### Comparación

```txt
==  !=  <  <=  >  >=
```

### Lógicos

```txt
&&  ||  !
```

### Punteros

```txt
&x      dirección de x
*p      valor apuntado por p
*p = v  asignación por desreferencia
```

## 6. Control de flujo

### Condicional

```et
if (x > 0) {
    print(x);
} else {
    print(0);
}
```

### Ciclo

```et
while (i < 10) {
    i = i + 1;
}
```

## 7. Funciones

```et
fun int suma(int a, int b) {
    return a + b;
}
```

Llamada:

```et
print(suma(2, 3));
```

## 8. Recursión

```et
fun int factorial(int n) {
    if (n == 0) {
        return 1;
    }
    return n * factorial(n - 1);
}
```

EduTrace permite usar `trace recursion;` para mostrar la evolución de llamadas recursivas.

## 9. Strings

```et
var string msg;
msg = "Hola EduTrace";
print(msg);
trace variables(msg);
```

Alcance actual:

- Sí se permiten literales string.
- Sí se permite imprimir strings.
- Sí se permite observar strings con `trace variables`.
- No se implementa concatenación de strings.

## 10. Chars y promoción automática

```et
var char c;
var int x;

c = 'A';
x = c + 1;

print(c);  // A
print(x);  // 66
```

Regla implementada:

```txt
char -> int en expresiones numéricas
```

No se permite asignar directamente un `int` a un `char`.

## 11. Arreglos

### Arreglo 1D

```et
var int nums[3];
nums[0] = 10;
nums[1] = 20;
nums[2] = nums[0] + nums[1];
print(nums[2]);
```

### Arreglo 2D

```et
var int mat[2][3];
mat[0][0] = 1;
mat[1][2] = 6;
print(mat[1][2]);
```

Reglas semánticas:

- El índice debe ser `int`.
- Si el índice es constante, se detecta fuera de rango en compilación.
- No se permite usar más índices que dimensiones declaradas.

## 12. Structs

```et
struct Student {
    int age;
    string name;
}

fun int main() {
    var Student s;
    s.age = 18;
    s.name = "Ana";

    print(s.name);
    print(s.age);
    return 0;
}
```

Reglas:

- El tipo struct debe estar declarado.
- El campo debe existir.
- El tipo asignado al campo debe ser compatible.

## 13. Punteros y memoria dinámica

### Punteros básicos

```et
var int x;
var int* p;

x = 10;
p = &x;
*p = 20;

print(x);
print(*p);
```

### Memoria dinámica

```et
var int* p;
p = new int;
*p = 50;
print(*p);
delete p;
```

Alcance actual:

- `new int`.
- `delete p`.
- No se implementa `new int[10]`.
- No se implementa `new Struct`.

## 14. Lambdas simples

```et
fun int main() {
    let inc = lambda int(int x) {
        return x + 1;
    };

    print(inc(5));
    return 0;
}
```

Alcance actual:

- Lambdas sin captura.
- Parámetros y retorno explícitos.
- Llamada mediante variable.
- No se implementan closures.

## 15. Templates simples

```et
fun T identity<T>(T x) {
    return x;
}

fun int main() {
    print(identity<int>(10));
    print(identity<string>("template"));
    print(identity<char>('Z'));
    return 0;
}
```

Alcance actual:

- Un parámetro genérico `T`.
- Llamada explícita con tipo: `identity<int>(10)`.
- Especialización en generación x86.
- No se implementan múltiples parámetros genéricos ni structs genéricos.

## 16. Directivas educativas

```et
trace variables(x, y);
trace branch;
trace loop(i, suma);
trace stack;
trace recursion;
trace array(nums);
trace memory(x, p);
```

Estas directivas no modifican la lógica del algoritmo; se usan para producir explicaciones educativas durante el modo `--trace`.
