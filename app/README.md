# EduTrace App

Aplicación mínima para demostrar el compilador EduTrace desde una interfaz web simple.

No usa frameworks ni dependencias externas. El backend está hecho con `http.server` de Python y el frontend con HTML, CSS y JavaScript puro.

## Requisitos

- Python 3
- g++
- gcc
- make

## Ejecutar

Desde la raíz del proyecto:

```bash
make
python3 app/backend/server.py
```

Luego abrir:

```txt
http://localhost:8000
```

También puedes usar:

```bash
make app
```

## Funciones disponibles

La app permite ejecutar estos modos del compilador:

- `--check`
- `--tokens`
- `--ast`
- `--trace`
- `--opt-report`
- `--asm`
- ejecución real del programa compilado con `gcc -no-pie`

## Estructura

```txt
app/
├── README.md
├── backend/
│   └── server.py
├── frontend/
│   ├── index.html
│   ├── styles.css
│   └── app.js
├── samples/
│   └── demo.et
└── tmp/
```

## Nota

Esta app es una versión funcional inicial. El objetivo es validar integración, no diseño visual final.
