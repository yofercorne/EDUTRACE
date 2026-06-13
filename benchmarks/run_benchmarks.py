#!/usr/bin/env python3
import csv
import os
import statistics
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BIN = ROOT / "build" / "edutrace"
OUT = ROOT / "benchmarks" / "results"
ET_DIR = ROOT / "benchmarks" / "edutrace"
C_DIR = ROOT / "benchmarks" / "c_equivalent"
TMP = ROOT / "build" / "benchmarks"

REPS = int(os.environ.get("BENCH_REPS", "3"))

OPTIMIZATION_FOCUS = {
    "constantes": "mixto: constant folding + if constante + while(false)",
    "constant_folding": "constant folding",
    "algebraic_simplification": "simplificación algebraica",
    "dead_code": "eliminación de código muerto",
    "if_constant": "if con condición constante",
    "while_false": "eliminación de while(false)",
    "mixed_optimization": "mixto: folding + álgebra + if + while + dead code",
    "factorial_iterativo": "control: ciclo dinámico",
    "factorial_recursivo": "control: recursión dinámica",
    "fibonacci": "control: recursión dinámica",
}


def run(cmd, cwd=ROOT):
    start = time.perf_counter()
    completed = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True)
    elapsed_ms = (time.perf_counter() - start) * 1000
    if completed.returncode != 0:
        raise RuntimeError(
            f"Comando falló: {' '.join(map(str, cmd))}\nSTDOUT:\n{completed.stdout}\nSTDERR:\n{completed.stderr}"
        )
    return elapsed_ms, completed.stdout.strip()


def mean_run(cmd):
    times = []
    last_out = ""
    for _ in range(REPS):
        elapsed, out = run(cmd)
        times.append(elapsed)
        last_out = out
    return statistics.mean(times), last_out


def count_nonempty_lines(path):
    if not path.exists():
        return 0
    return sum(1 for line in path.read_text(errors="ignore").splitlines() if line.strip())


def ensure_build():
    if not BIN.exists():
        run(["make"])
    OUT.mkdir(parents=True, exist_ok=True)
    TMP.mkdir(parents=True, exist_ok=True)


def get_opt_report(source):
    _, report = run([str(BIN), str(source), "--opt-report"])
    return " | ".join(line.strip() for line in report.splitlines() if line.strip())


def bench_edutrace(source, optimize):
    stem = source.stem
    mode_name = "EduTrace-O1" if optimize else "EduTrace-O0"
    asm = TMP / f"{stem}_{'opt' if optimize else 'noopt'}.s"
    exe = TMP / f"{stem}_{'opt' if optimize else 'noopt'}"
    mode = "--asm" if optimize else "--asm-no-opt"

    compile_ms, _ = run([str(BIN), str(source), mode, str(asm)])
    gcc_ms, _ = run(["gcc", "-no-pie", str(asm), "-o", str(exe)])
    run_ms, output = mean_run([str(exe)])

    return {
        "benchmark": stem,
        "category": OPTIMIZATION_FOCUS.get(stem, "general"),
        "tool": mode_name,
        "compile_ms": f"{compile_ms + gcc_ms:.3f}",
        "run_ms_avg": f"{run_ms:.3f}",
        "output": output.replace("\n", " | "),
        "asm_lines": count_nonempty_lines(asm),
        "opt_report": get_opt_report(source) if optimize else "",
    }


def bench_c(source, opt):
    stem = source.stem
    tool = f"GCC-{opt}"
    exe = TMP / f"{stem}_gcc_{opt}"
    compile_ms, _ = run(["gcc", f"-{opt}", str(source), "-o", str(exe)])
    run_ms, output = mean_run([str(exe)])
    return {
        "benchmark": stem,
        "category": OPTIMIZATION_FOCUS.get(stem, "general"),
        "tool": tool,
        "compile_ms": f"{compile_ms:.3f}",
        "run_ms_avg": f"{run_ms:.3f}",
        "output": output.replace("\n", " | "),
        "asm_lines": "",
        "opt_report": "",
    }


def pct_reduction(before, after):
    if not before:
        return "0.00"
    return f"{((before - after) / before) * 100:.2f}"


def pct_speedup(before_ms, after_ms):
    before = float(before_ms)
    after = float(after_ms)
    if before <= 0:
        return "0.00"
    return f"{((before - after) / before) * 100:.2f}"


def build_optimization_summary(rows):
    by_benchmark = {}
    for row in rows:
        if row["tool"] in {"EduTrace-O0", "EduTrace-O1"}:
            by_benchmark.setdefault(row["benchmark"], {})[row["tool"]] = row

    summary = []
    for benchmark, data in sorted(by_benchmark.items()):
        if "EduTrace-O0" not in data or "EduTrace-O1" not in data:
            continue
        o0 = data["EduTrace-O0"]
        o1 = data["EduTrace-O1"]
        asm_o0 = int(o0["asm_lines"])
        asm_o1 = int(o1["asm_lines"])
        summary.append({
            "benchmark": benchmark,
            "optimization_focus": o1["category"],
            "output_o0": o0["output"],
            "output_o1": o1["output"],
            "same_output": "yes" if o0["output"] == o1["output"] else "no",
            "asm_o0": asm_o0,
            "asm_o1": asm_o1,
            "asm_reduction_lines": asm_o0 - asm_o1,
            "asm_reduction_pct": pct_reduction(asm_o0, asm_o1),
            "run_o0_ms": o0["run_ms_avg"],
            "run_o1_ms": o1["run_ms_avg"],
            "run_speedup_pct": pct_speedup(o0["run_ms_avg"], o1["run_ms_avg"]),
            "opt_report": o1["opt_report"],
        })
    return summary


def main():
    ensure_build()
    rows = []
    for et in sorted(ET_DIR.glob("*.et")):
        rows.append(bench_edutrace(et, optimize=False))
        rows.append(bench_edutrace(et, optimize=True))
        c_file = C_DIR / f"{et.stem}.c"
        if c_file.exists():
            rows.append(bench_c(c_file, "O0"))
            rows.append(bench_c(c_file, "O1"))

    fieldnames = ["benchmark", "category", "tool", "compile_ms", "run_ms_avg", "output", "asm_lines", "opt_report"]
    out_csv = OUT / "benchmark_results.csv"
    with out_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)

    summary = build_optimization_summary(rows)
    summary_csv = OUT / "optimization_effects.csv"
    summary_fields = [
        "benchmark", "optimization_focus", "output_o0", "output_o1", "same_output",
        "asm_o0", "asm_o1", "asm_reduction_lines", "asm_reduction_pct",
        "run_o0_ms", "run_o1_ms", "run_speedup_pct", "opt_report"
    ]
    with summary_csv.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=summary_fields)
        writer.writeheader()
        writer.writerows(summary)

    print(f"Resultados guardados en {out_csv}")
    print(f"Resumen de optimización guardado en {summary_csv}")
    print(f"Repeticiones por ejecución: {REPS}")
    print()
    print("== Resumen O0 vs O1 ==")
    for row in summary:
        print(
            f"{row['benchmark']:28s} asm {row['asm_o0']:>4} -> {row['asm_o1']:<4} "
            f"reducción={row['asm_reduction_pct']:>6s}% salida={row['same_output']}"
        )

    print()
    print("== Resultados completos ==")
    for row in rows:
        print(
            f"{row['benchmark']:28s} {row['tool']:12s} "
            f"compile={row['compile_ms']:>9s}ms run={row['run_ms_avg']:>9s}ms output={row['output']}"
        )


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:
        print(exc, file=sys.stderr)
        sys.exit(1)
