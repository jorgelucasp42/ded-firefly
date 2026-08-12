#!/usr/bin/env bash
# Fase E — Desempenho final
# Configuração única (opção B): população 320 (Fase B), sistema 100 unidades
# (Fase C/D), 1000 iterações, semente fixa 42 (mesmo problema repetido —
# aqui o objetivo é medir ruído de sistema no tempo, não variância do
# algoritmo, que já é papel da Fase D).
#
# Para cada uma das 7 versões (SEQ, OMP com 1/2/4/8/12 threads, ACC_SYNC):
# 1 execução de aquecimento (descartada) + 10 repetições cronometradas.
#
# Uso:
#   ./run_phase_e.sh          # 10 repeticoes completas
#   ./run_phase_e.sh --pilot  # so 3 repeticoes, pra validar o pipeline
#
# Saídas:
#   results/phase_e/<timestamp>/manifest.txt
#   results/phase_e/<timestamp>/summary.csv   (so as repeticoes cronometradas, aquecimento descartado)
#   results/phase_e/<timestamp>/stats.csv     (mediana/IQR/media/desvio por versao + speedup/eficiencia)
#   results/phase_e/<timestamp>/logs/*.log

set -euo pipefail

PILOT=0
if [[ "${1:-}" == "--pilot" ]]; then
    PILOT=1
fi

POP=320
SYSTEM=100
ITERS=1000
SEED=42
N_REPS=10

if [[ "$PILOT" -eq 1 ]]; then
    N_REPS=3
    echo "== MODO PILOTO (so 3 repeticoes por versao, valida o pipeline) =="
fi

REPETICAO_BASE=1
DATASET_NAME="Victoire${SYSTEM}"

STAMP=$(date +%Y%m%d_%H%M%S)
SUFFIX=""
[[ "$PILOT" -eq 1 ]] && SUFFIX="_pilot"
OUT_DIR="results/phase_e/${STAMP}${SUFFIX}"
LOG_DIR="${OUT_DIR}/logs"
MANIFEST="${OUT_DIR}/manifest.txt"
SUMMARY="${OUT_DIR}/summary.csv"
STATS="${OUT_DIR}/stats.csv"

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "NA")

mkdir -p "$LOG_DIR"

echo "== Fase E: compilando binarios =="
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
        echo "n_repeticoes=${N_REPS}"
        echo "semente_fixa=${SEED}"
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
    echo "timestamp,commit,phase,dataset,version,n_units,n_hours,n_vars,n_fireflies,iters,seed,repeticao,threads,alpha,beta0,gamma,cost,time_s,t_kernel_s,t_pcie_s,t_cpu_s,valid_ok,max_balance,max_pmin,max_pmax,max_ramp_up,max_ramp_down,nan,inf,observacoes,log_file" > "$SUMMARY"
}

run_once() {
    local bin=$1
    local threads=$2
    local omp_env=$3
    local log_file=$4

    if [[ -n "$omp_env" ]]; then
        OMP_NUM_THREADS="$omp_env" "$bin" "$POP" "$SEED" "$ITERS" "$SYSTEM" > "$log_file" 2>&1
    else
        "$bin" "$POP" "$SEED" "$ITERS" "$SYSTEM" > "$log_file" 2>&1
    fi
}

record_row() {
    local version=$1
    local rep=$2
    local threads=$3
    local log_file=$4

    local cost time_s alpha beta0 gamma n_units n_hours n_vars valid
    local max_balance max_pmin max_pmax max_ramp_up max_ramp_down nan_count inf_count
    local t_kernel t_pcie t_cpu
    local ts
    ts=$(date -Iseconds)
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
    t_cpu=$(extract_time_block "CPU \\(noise\\+trial\\+best\\)" "$log_file")

    echo "${ts},${COMMIT},E,${DATASET_NAME},${version},${n_units},${n_hours},${n_vars},${POP},${ITERS},${SEED},${rep},${threads},${alpha},${beta0},${gamma},${cost},${time_s},${t_kernel},${t_pcie},${t_cpu},${valid},${max_balance},${max_pmin},${max_pmax},${max_ramp_up},${max_ramp_down},${nan_count},${inf_count},,${log_file}" >> "$SUMMARY"
}

run_config() {
    local version=$1
    local bin=$2
    local threads=$3
    local omp_env=$4
    local tag=$5

    echo "== ${tag}: aquecimento =="
    run_once "$bin" "$threads" "$omp_env" "${LOG_DIR}/${tag}_warmup.log"

    for rep in $(seq 1 "$N_REPS"); do
        echo "-> ${tag} repeticao ${rep}/${N_REPS}"
        local log_file="${LOG_DIR}/${tag}_rep${rep}.log"
        run_once "$bin" "$threads" "$omp_env" "$log_file"
        record_row "$version" "$rep" "$threads" "$log_file"
    done
}

write_summary_header

T_START=$(date +%s)

run_config "SEQ" "./bin/bin_seq_bench" "1" "" "seq"
run_config "OMP" "./bin/bin_omp_bench" "1" "1" "omp_t1"
run_config "OMP" "./bin/bin_omp_bench" "2" "2" "omp_t2"
run_config "OMP" "./bin/bin_omp_bench" "4" "4" "omp_t4"
run_config "OMP" "./bin/bin_omp_bench" "8" "8" "omp_t8"
run_config "OMP" "./bin/bin_omp_bench" "12" "12" "omp_t12"
run_config "ACC_SYNC" "./bin/bin_acc_sync_bench" "GPU" "" "acc_sync"

T_END=$(date +%s)
ELAPSED=$((T_END - T_START))

echo "== calculando estatisticas, speedup e eficiencia =="
python3 - "$SUMMARY" "$STATS" <<'PYEOF'
import csv, statistics, sys

summary_path, stats_path = sys.argv[1], sys.argv[2]
rows = list(csv.DictReader(open(summary_path)))

# agrupa por (version, threads) na ordem de execucao
configs = []
seen = set()
for r in rows:
    key = (r["version"], r["threads"])
    if key not in seen:
        seen.add(key)
        configs.append(key)

def q(data, which):
    qs = statistics.quantiles(data, n=4, method="inclusive")
    return {"q1": qs[0], "q3": qs[2]}[which]

stats_by_config = {}
for version, threads in configs:
    crows = [r for r in rows if r["version"] == version and r["threads"] == threads]
    times = [float(r["time_s"]) for r in crows]
    costs = [float(r["cost"]) for r in crows]
    median_t = statistics.median(times)
    q1, q3 = q(times, "q1"), q(times, "q3")
    iqr = q3 - q1
    mean_t = statistics.mean(times)
    stdev_t = statistics.stdev(times) if len(times) > 1 else 0.0
    stats_by_config[(version, threads)] = {
        "n": len(times), "median_s": median_t, "q1_s": q1, "q3_s": q3, "iqr_s": iqr,
        "mean_s": mean_t, "stdev_s": stdev_t,
        "min_s": min(times), "max_s": max(times),
        "cost_estavel": len(set(f"{c:.4f}" for c in costs)) == 1,
    }
    if version == "ACC_SYNC":
        tk = [float(r["t_kernel_s"]) for r in crows if r["t_kernel_s"] != "NA"]
        tp = [float(r["t_pcie_s"]) for r in crows if r["t_pcie_s"] != "NA"]
        tc = [float(r["t_cpu_s"]) for r in crows if r["t_cpu_s"] != "NA"]
        if tk: stats_by_config[(version, threads)]["median_kernel_s"] = statistics.median(tk)
        if tp: stats_by_config[(version, threads)]["median_pcie_s"] = statistics.median(tp)
        if tc: stats_by_config[(version, threads)]["median_cpu_s"] = statistics.median(tc)

seq_median = stats_by_config[("SEQ", "1")]["median_s"]

with open(stats_path, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["version", "threads", "n", "median_s", "q1_s", "q3_s", "iqr_s",
                "mean_s", "stdev_s", "min_s", "max_s", "cost_estavel_entre_repeticoes",
                "speedup_vs_seq", "eficiencia_pct",
                "median_kernel_s", "median_pcie_s", "median_cpu_s"])
    for version, threads in configs:
        s = stats_by_config[(version, threads)]
        speedup = seq_median / s["median_s"]
        eff = ""
        if version == "OMP":
            eff = f"{speedup/int(threads)*100:.1f}"
        w.writerow([version, threads, s["n"], f"{s['median_s']:.4f}", f"{s['q1_s']:.4f}",
                    f"{s['q3_s']:.4f}", f"{s['iqr_s']:.4f}", f"{s['mean_s']:.4f}",
                    f"{s['stdev_s']:.4f}", f"{s['min_s']:.4f}", f"{s['max_s']:.4f}",
                    s["cost_estavel"], f"{speedup:.3f}", eff,
                    f"{s.get('median_kernel_s',''):.4f}" if "median_kernel_s" in s else "",
                    f"{s.get('median_pcie_s',''):.4f}" if "median_pcie_s" in s else "",
                    f"{s.get('median_cpu_s',''):.4f}" if "median_cpu_s" in s else ""])
print(open(stats_path).read())
PYEOF

echo
echo "== Fase E ($([[ $PILOT -eq 1 ]] && echo PILOTO || echo COMPLETA)) concluida =="
echo "Tempo total:            ${ELAPSED}s (~$((ELAPSED/60)) min)"
echo "Manifesto de ambiente:  $MANIFEST"
echo "Resumo principal:       $SUMMARY"
echo "Estatisticas:           $STATS"
echo "Logs brutos:            $LOG_DIR"
