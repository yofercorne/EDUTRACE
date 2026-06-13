#!/usr/bin/env python3
"""
Servidor simple para la app de EduTrace.
No requiere Flask ni dependencias externas: usa solo la biblioteca estándar de Python.

Uso:
  cd EduTrace
  make
  python3 app/backend/server.py

Luego abrir:
  http://localhost:8000
"""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse
import json
import os
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[2]
FRONTEND = ROOT / "app" / "frontend"
TMP_DIR = ROOT / "app" / "tmp"
COMPILER = ROOT / "build" / "edutrace"
DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 8000

TMP_DIR.mkdir(parents=True, exist_ok=True)

MIME_TYPES = {
    ".html": "text/html; charset=utf-8",
    ".css": "text/css; charset=utf-8",
    ".js": "application/javascript; charset=utf-8",
    ".json": "application/json; charset=utf-8",
    ".txt": "text/plain; charset=utf-8",
}

ACTIONS = {
    "tokens": "--tokens",
    "ast": "--ast",
    "check": "--check",
    "trace": "--trace",
    "opt_ast": "--opt-ast",
    "opt_report": "--opt-report",
    "asm": "--asm",
    "asm_no_opt": "--asm-no-opt",
    "run": "--asm",
}


def run_command(args, timeout=10):
    """Ejecuta un comando dentro de la raíz del proyecto."""
    start = time.perf_counter()
    try:
        result = subprocess.run(
            args,
            cwd=ROOT,
            text=True,
            capture_output=True,
            timeout=timeout,
        )
        elapsed_ms = round((time.perf_counter() - start) * 1000, 3)
        return {
            "ok": result.returncode == 0,
            "returncode": result.returncode,
            "stdout": result.stdout,
            "stderr": result.stderr,
            "elapsed_ms": elapsed_ms,
            "command": " ".join(str(a) for a in args),
        }
    except subprocess.TimeoutExpired as exc:
        elapsed_ms = round((time.perf_counter() - start) * 1000, 3)
        return {
            "ok": False,
            "returncode": -1,
            "stdout": exc.stdout or "",
            "stderr": "Tiempo de ejecución excedido.",
            "elapsed_ms": elapsed_ms,
            "command": " ".join(str(a) for a in args),
        }


def ensure_compiler():
    if COMPILER.exists():
        return {"ok": True, "stdout": "", "stderr": ""}
    return run_command(["make"], timeout=30)


def compile_request(code, action):
    if action not in ACTIONS:
        return {
            "ok": False,
            "error": f"Acción desconocida: {action}",
            "stdout": "",
            "stderr": "",
        }

    build = ensure_compiler()
    if not build.get("ok"):
        return {
            "ok": False,
            "error": "No se pudo compilar build/edutrace con make.",
            "stdout": build.get("stdout", ""),
            "stderr": build.get("stderr", ""),
            "build": build,
        }

    with tempfile.TemporaryDirectory(dir=TMP_DIR) as tmp:
        tmp_path = Path(tmp)
        source_path = tmp_path / "program.et"
        asm_path = tmp_path / "program.s"
        exe_path = tmp_path / "program.out"
        source_path.write_text(code, encoding="utf-8")

        mode = ACTIONS[action]

        if action in {"asm", "asm_no_opt"}:
            result = run_command([str(COMPILER), str(source_path), mode, str(asm_path)])
            asm_text = asm_path.read_text(encoding="utf-8") if asm_path.exists() else ""
            result["asm"] = asm_text
            return result

        if action == "run":
            asm_result = run_command([str(COMPILER), str(source_path), "--asm", str(asm_path)])
            if not asm_result["ok"]:
                asm_result["asm"] = asm_path.read_text(encoding="utf-8") if asm_path.exists() else ""
                return asm_result

            gcc_result = run_command(["gcc", "-no-pie", str(asm_path), "-o", str(exe_path)], timeout=15)
            if not gcc_result["ok"]:
                return {
                    "ok": False,
                    "stdout": asm_result.get("stdout", "") + gcc_result.get("stdout", ""),
                    "stderr": gcc_result.get("stderr", ""),
                    "asm": asm_path.read_text(encoding="utf-8") if asm_path.exists() else "",
                    "compile": asm_result,
                    "gcc": gcc_result,
                }

            run_result = run_command([str(exe_path)], timeout=10)
            return {
                "ok": run_result["ok"],
                "stdout": run_result.get("stdout", ""),
                "stderr": run_result.get("stderr", ""),
                "asm": asm_path.read_text(encoding="utf-8") if asm_path.exists() else "",
                "compile": asm_result,
                "gcc": gcc_result,
                "run": run_result,
                "elapsed_ms": run_result.get("elapsed_ms", 0),
            }

        return run_command([str(COMPILER), str(source_path), mode])


class EduTraceHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urlparse(self.path)
        if parsed.path == "/":
            return self.serve_file(FRONTEND / "index.html")
        if parsed.path.startswith("/frontend/"):
            rel = parsed.path.replace("/frontend/", "", 1)
            return self.serve_file(FRONTEND / rel)
        if parsed.path == "/api/health":
            return self.send_json({"ok": True, "compiler": str(COMPILER), "exists": COMPILER.exists()})
        self.send_error(404, "Ruta no encontrada")

    def do_POST(self):
        parsed = urlparse(self.path)
        if parsed.path != "/api/compile":
            self.send_error(404, "Ruta no encontrada")
            return

        try:
            length = int(self.headers.get("Content-Length", "0"))
            raw = self.rfile.read(length).decode("utf-8")
            body = json.loads(raw or "{}")
            code = body.get("code", "")
            action = body.get("action", "check")

            if not isinstance(code, str) or not code.strip():
                return self.send_json({"ok": False, "error": "El código está vacío."}, status=400)

            result = compile_request(code, action)
            self.send_json(result, status=200 if result.get("ok") else 400)
        except Exception as exc:
            self.send_json({"ok": False, "error": str(exc)}, status=500)

    def serve_file(self, path):
        path = Path(path).resolve()
        if not str(path).startswith(str(FRONTEND.resolve())) or not path.exists() or not path.is_file():
            self.send_error(404, "Archivo no encontrado")
            return

        content = path.read_bytes()
        mime = MIME_TYPES.get(path.suffix, "application/octet-stream")
        self.send_response(200)
        self.send_header("Content-Type", mime)
        self.send_header("Content-Length", str(len(content)))
        self.end_headers()
        self.wfile.write(content)

    def send_json(self, data, status=200):
        payload = json.dumps(data, ensure_ascii=False, indent=2).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

    def log_message(self, fmt, *args):
        print("[EduTrace App] " + fmt % args)


def main():
    host = os.environ.get("EDUTRACE_APP_HOST", DEFAULT_HOST)
    port = int(os.environ.get("EDUTRACE_APP_PORT", DEFAULT_PORT))
    server = ThreadingHTTPServer((host, port), EduTraceHandler)
    print(f"EduTrace App ejecutándose en http://{host}:{port}")
    print("Presiona Ctrl+C para detener.")
    server.serve_forever()


if __name__ == "__main__":
    main()
