#!/usr/bin/env python3
"""
consolidate_bench.py — consolida os arquivos bench_f*.txt (saída de
bench_compare.sh) em uma única tabela CSV, pronta para análise estatística
e para gerar tabelas/gráficos do TCC.

Uso:
    python3 scripts/consolidate_bench.py [glob] [-o saida.csv]

Exemplo:
    python3 scripts/consolidate_bench.py "bench_f*.txt" -o docs/resultados_consolidados.csv

Cada arquivo bench_f*.txt pode ter dois formatos (gerados por
bench_compare.sh), dependendo se o binário OMP foi rodado:

  Formato completo (seq + omp + acc):
    f=320 seed=42 iters=1000 threads=12
    seq_cost=...  seq_time=...
    omp_cost=...  omp_time=...
    acc_cost=...  acc_time=...
    speedup_omp=5,37x  speedup_acc=7,44x
    gap_omp=0,0000%  gap_acc=-0,6442%

  Formato reduzido (seq + acc, sem omp/threads):
    f=120 seed=42 iters=5000
    seq_cost=...  seq_time=...
    acc_cost=...  acc_time=...
    speedup=3,47x  gap=-0,6066%

O script normaliza os dois formatos para as mesmas colunas, deixando vazio
(NA) o que não existir em cada arquivo (ex.: omp_cost/omp_time/threads no
formato reduzido).
"""

import argparse
import csv
import glob
import re
import sys
from pathlib import Path


def to_float(s):
    """Converte string numérica (usando ',' ou '.' como separador decimal) para float."""
    if s is None:
        return None
    s = s.strip().rstrip("x%").replace(",", ".")
    try:
        return float(s)
    except ValueError:
        return None


# Cada padrão captura um par chave=valor em qualquer lugar do arquivo.
PATTERNS = {
    "n_fireflies": r"f=(\d+)",
    "seed": r"seed=(\d+)",
    "iters": r"iters=(\d+)",
    "threads": r"threads=(\d+)",
    "seq_cost": r"seq_cost=([\d.,]+)",
    "seq_time": r"seq_time=([\d.,]+)",
    "omp_cost": r"omp_cost=([\d.,]+)",
    "omp_time": r"omp_time=([\d.,]+)",
    "acc_cost": r"acc_cost=([\d.,]+)",
    "acc_time": r"acc_time=([\d.,]+)",
    "speedup_omp": r"speedup_omp=([\d.,]+)x",
    "speedup_acc": r"speedup_acc=([\d.,]+)x",
    "gap_omp": r"gap_omp=(-?[\d.,]+)%",
    "gap_acc": r"gap_acc=(-?[\d.,]+)%",
    # formato reduzido (sem "_omp"/"_acc" no nome, só seq + acc)
    "speedup_simple": r"(?<!_)speedup=([\d.,]+)x",
    "gap_simple": r"(?<!_)gap=(-?[\d.,]+)%",
}

FLOAT_FIELDS = {
    "seq_cost", "seq_time", "omp_cost", "omp_time", "acc_cost", "acc_time",
    "speedup_omp", "speedup_acc", "gap_omp", "gap_acc",
    "speedup_simple", "gap_simple",
}
INT_FIELDS = {"n_fireflies", "seed", "iters", "threads"}

CSV_COLUMNS = [
    "arquivo", "n_fireflies", "n_unidades", "seed", "iters", "threads",
    "seq_cost", "seq_time",
    "omp_cost", "omp_time", "speedup_omp", "gap_omp_pct",
    "acc_cost", "acc_time", "speedup_acc", "gap_acc_pct",
]


def parse_file(path: Path) -> dict:
    text = path.read_text(encoding="utf-8")
    row = {"arquivo": path.name}
    for key, pattern in PATTERNS.items():
        m = re.search(pattern, text)
        if not m:
            row[key] = None
            continue
        val = m.group(1)
        row[key] = to_float(val) if key in FLOAT_FIELDS else int(val)

    # Formato reduzido (só seq+acc): "speedup"/"gap" viram speedup_acc/gap_acc
    if row.get("speedup_acc") is None and row.get("speedup_simple") is not None:
        row["speedup_acc"] = row["speedup_simple"]
    if row.get("gap_acc") is None and row.get("gap_simple") is not None:
        row["gap_acc"] = row["gap_simple"]

    # n_unidades não está nos bench_f*.txt atuais (todos usam o dataset padrão
    # de 100 unidades, conforme main_*_bench.c). Deixe explícito aqui em vez de
    # assumir silenciosamente — ajuste se algum bench for rodado com outro dataset.
    row["n_unidades"] = 100

    row["gap_omp_pct"] = row.pop("gap_omp", None)
    row["gap_acc_pct"] = row.pop("gap_acc", None)
    return row


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("pattern", nargs="?", default="bench_f*.txt",
                    help="Glob dos arquivos de benchmark (padrão: bench_f*.txt)")
    ap.add_argument("-o", "--output", default="docs/resultados_consolidados.csv",
                    help="Arquivo CSV de saída")
    args = ap.parse_args()

    files = sorted(Path(p) for p in glob.glob(args.pattern))
    if not files:
        print(f"Nenhum arquivo encontrado para o padrão '{args.pattern}'.", file=sys.stderr)
        sys.exit(1)

    rows = [parse_file(f) for f in files]

    out_path = Path(args.output)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open("w", newline="", encoding="utf-8") as fh:
        writer = csv.DictWriter(fh, fieldnames=CSV_COLUMNS, extrasaction="ignore")
        writer.writeheader()
        for r in rows:
            writer.writerow(r)

    print(f"{len(rows)} arquivo(s) consolidado(s) em {out_path}")


if __name__ == "__main__":
    main()
