#!/usr/bin/env bash
# Fase D — Qualidade da solução
# Configuração travada a partir das fases anteriores: população 320
# (Fase B), sistema de 100 unidades (onde a Fase C mediu o maior speedup
# relativo do OpenACC), 1000 iterações (consistente com B e C).
#
# 30 sementes fixas x 3 versões (SEQ, OMP@12, ACC_SYNC) = 90 execuções.
#
# Uso:
#   ./run_phase_d.sh          # 30 sementes completas
#   ./run_phase_d.sh --pilot  # so 3 sementes, pra validar o pipeline
#
# Saídas:
#   results/phase_d/<timestamp>/manifest.txt
#   results/phase_d/<timestamp>/summary.csv
#   results/phase_d/<timestamp>/stats.csv      (melhor/media/mediana/desvio/pior/CV/factibilidade por versao)
#   results/phase_d/<timestamp>/logs/*.log

set -euo pipefail

PILOT=0
if [[ "${1:-}" == "--pilot" ]]; then
    PILOT=1
fi

POP=320
SYSTEM=100
ITERS=1000
OMP_THREADS=12
SEEDS=(42 123 777 1234 9999 2024 5555 8080 271 63 \
       11 17 29 53 101 202 303 404 505 606 \
       707 808 909 1111 2222 3333 4444 6789 8642 13579)

if [[ "$PILOT" -eq 1 ]]; then
    SEEDS=(42 123 777)
    echo "== MODO PILOTO (so 3 sementes, valida o pipeline) =="
fi

REPETICAO=1
DATASET_NAME="Victoire${SYSTEM}"

STAMP=$(date +%Y%m%d_%H%M%S)
SUFFIX=""
[[ "$PILOT" -eq 1 ]] && SUFFIX="_pilot"
OUT_DIR="results/phase_d/${STAMP}${SUFFIX}"
LOG_DIR="${OUT_DIR}/logs"
MANIFEST="${OUT_DIR}/manifest.txt"
SUMMARY="${OUT_DIR}/summary.csv"
STATS="${OUT_DIR}/stats.csv"

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "NA")

mkdir -p "$LOG_DIR"

echo "== Fase D: compilando binarios =="
make bench_seq bench_omp bench_acc_sync

write_manifest() {
    {
        echo "data_hora=$(date -Iseconds)"
        echo "commit=${COMMIT}"
        echo "so_kernel=$(uname -a)"
        echo "cpu_modelo=$(LC_ALL=C lscpu 2>/dev/null | grep -m1 '^Model name:' | sed 's/Model name:\s*//' || echo NA)"
        echo "cpu_nucleos_fisicos=$(LC_ALL=C lscpu 2>/dev/null | grep -m1 '^Core(s) per socket:' | awk -F: '{print $2}' | xargs || echo NA)"
        echo "cpu_threads_logicos=$(nproc)"
        if command -v nvidia-smi >/dev/null 2>&1; then
            echo "gpu_modelo=$(nvidia-smi --query-gpu=name --format=csv,noheader 2>/dev/null | head -1)"
            echo "gpu_memoria=$(nvidia-smi --query-gpu=memory.total --format=csv,noheader 2>/dev/null | head -1)"
            echo "gpu_driver=$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1)"
        else
            echo "gpu_modelo=NA (nvidia-smi indisponivel)"
            echo "gpu_memoria=NA"
            echo "gpu_driver=NA"
        fi
        echo "ram_total=$(LC_ALL=C free -h 2>/dev/null | awk '/^Mem:/{print $2}' || echo NA)"
        echo "gcc_versao=$(gcc --version 2>/dev/null | head -1 || echo NA)"
        echo "nvc_versao=$(nvc --version 2>/dev/null | head -2 | tail -1 || echo NA)"
        echo "cflags=$(grep -m1 '^CFLAGS=' Makefile)"
        echo "acc_flags=$(grep -m1 '^ACC_FLAGS' Makefile)"
        echo "load_average=$(uptime 2>/dev/null | sed 's/.*load average: //' || echo NA)"
        echo "modo_piloto=${PILOT}"
        echo "# estado energetico e processos concorrentes: preencher manualmente se relevante"
    } > "$MANIFEST"
}
write_manifest

extract_field() {
    local field=$1
    local file=$2
    grep -m1 -oP "${field}=\\K[-+0-9.eE]+" "$file" || echo "NA"
}

extract_time_block() {
    local label=$1
    local file=$2
    grep -m1 -oP "${label}.*?:\s+\K[\d.]+(?=\s*s)" "$file" || echo "NA"
}

write_summary_header() {
    echo "timestamp,commit,phase,dataset,version,n_units,n_hours,n_vars,n_fireflies,iters,seed,repeticao,threads,alpha,beta0,gamma,cost,time_s,t_kernel_s,t_pcie_s,valid_ok,max_balance,max_pmin,max_pmax,max_ramp_up,max_ramp_down,nan,inf,observacoes,log_file" > "$SUMMARY"
}

run_version() {
    local version=$1
    local bin=$2
    local seed=$3
    local threads=$4
    local omp_env=$5
    local log_file=$6

    echo "-> ${version} seed=${seed}"

    local ts
    ts=$(date -Iseconds)

    if [[ -n "$omp_env" ]]; then
        OMP_NUM_THREADS="$omp_env" "$bin" "$POP" "$seed" "$ITERS" "$SYSTEM" > "$log_file" 2>&1
    else
        "$bin" "$POP" "$seed" "$ITERS" "$SYSTEM" > "$log_file" 2>&1
    fi

    local cost time_s alpha beta0 gamma n_units n_hours n_vars valid
    local max_balance max_pmin max_pmax max_ramp_up max_ramp_down nan_count inf_count
    local t_kernel t_pcie
    cost=$(extract_field RESULT_COST "$log_file")
    time_s=$(extract_field RESULT_TIME "$log_file")
    alpha=$(extract_field RESULT_ALPHA "$log_file")
    beta0=$(extract_field RESULT_BETA0 "$log_file")
    gamma=$(extract_field RESULT_GAMMA "$log_file")
    n_units=$(extract_field RESULT_N_UNITS "$log_file")
    n_hours=$(extract_field RESULT_N_HOURS "$log_file")
    n_vars=$(awk -v u="$n_units" -v h="$n_hours" 'BEGIN { if (u ~ /^[0-9]+$/ && h ~ /^[0-9]+$/) print u*h; else print "NA" }')
    valid=$(extract_field RESULT_VALID_OK "$log_file")
    max_balance=$(extract_field RESULT_MAX_BALANCE "$log_file")
    max_pmin=$(extract_field RESULT_MAX_PMIN "$log_file")
    max_pmax=$(extract_field RESULT_MAX_PMAX "$log_file")
    max_ramp_up=$(extract_field RESULT_MAX_RAMP_UP "$log_file")
    max_ramp_down=$(extract_field RESULT_MAX_RAMP_DOWN "$log_file")
    nan_count=$(extract_field RESULT_NAN "$log_file")
    inf_count=$(extract_field RESULT_INF "$log_file")
    t_kernel=$(extract_time_block "Kernels GPU" "$log_file")
    t_pcie=$(extract_time_block "Transferencias PCIe" "$log_file")

    echo "${ts},${COMMIT},D,${DATASET_NAME},${version},${n_units},${n_hours},${n_vars},${POP},${ITERS},${seed},${REPETICAO},${threads},${alpha},${beta0},${gamma},${cost},${time_s},${t_kernel},${t_pcie},${valid},${max_balance},${max_pmin},${max_pmax},${max_ramp_up},${max_ramp_down},${nan_count},${inf_count},,${log_file}" >> "$SUMMARY"
}

write_summary_header

RUN_COUNT=0
T_START=$(date +%s)

for seed in "${SEEDS[@]}"; do
    seq_log="${LOG_DIR}/seq_s${seed}.log"
    run_version "SEQ" "./bin/bin_seq_bench" "$seed" "1" "" "$seq_log"
    RUN_COUNT=$((RUN_COUNT+1))

    omp_log="${LOG_DIR}/omp_t${OMP_THREADS}_s${seed}.log"
    run_version "OMP" "./bin/bin_omp_bench" "$seed" "$OMP_THREADS" "$OMP_THREADS" "$omp_log"
    RUN_COUNT=$((RUN_COUNT+1))

    acc_log="${LOG_DIR}/acc_sync_s${seed}.log"
    run_version "ACC_SYNC" "./bin/bin_acc_sync_bench" "$seed" "GPU" "" "$acc_log"
    RUN_COUNT=$((RUN_COUNT+1))
done

T_END=$(date +%s)
ELAPSED=$((T_END - T_START))

echo "== calculando estatisticas =="
python3 - "$SUMMARY" "$STATS" <<'PYEOF'
import csv, statistics, sys

summary_path, stats_path = sys.argv[1], sys.argv[2]
rows = list(csv.DictReader(open(summary_path)))

versions = ["SEQ", "OMP", "ACC_SYNC"]
with open(stats_path, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["version", "n", "feasible_n", "feasible_rate_pct",
                "best_cost", "worst_cost", "mean_cost", "median_cost",
                "stdev_cost", "cv_pct"])
    for v in versions:
        vrows = [r for r in rows if r["version"] == v]
        n = len(vrows)
        feas = [r for r in vrows if r["valid_ok"] == "1"]
        costs = [float(r["cost"]) for r in vrows]
        best = min(costs)
        worst = max(costs)
        mean = statistics.mean(costs)
        median = statistics.median(costs)
        stdev = statistics.stdev(costs) if n > 1 else 0.0
        cv = (stdev / mean * 100) if mean else 0.0
        w.writerow([v, n, len(feas), f"{len(feas)/n*100:.2f}",
                    f"{best:.6f}", f"{worst:.6f}", f"{mean:.6f}", f"{median:.6f}",
                    f"{stdev:.6f}", f"{cv:.4f}"])
print(open(stats_path).read())
PYEOF

echo
echo "== Fase D ($([[ $PILOT -eq 1 ]] && echo PILOTO || echo COMPLETA)) concluida =="
echo "Execucoes:              $RUN_COUNT"
echo "Tempo total:            ${ELAPSED}s (~$((ELAPSED/60)) min)"
echo "Manifesto de ambiente:  $MANIFEST"
echo "Resumo principal:       $SUMMARY"
echo "Estatisticas:           $STATS"
echo "Logs brutos:            $LOG_DIR"
