#!/usr/bin/env bash
# Fase A — Correção e reprodutibilidade
# Sistema: 100 unidades, 24 horas
#
# Matriz principal:
#   populações: 60, 120
#   iterações: 100, 500
#   sementes: 42, 123, 777
#   versões: SEQ, OMP, ACC_SYNC
#
# Saídas:
#   results/phase_a/<timestamp>/manifest.txt
#   results/phase_a/<timestamp>/summary.csv
#   results/phase_a/<timestamp>/seq_omp_equivalence.csv
#   results/phase_a/<timestamp>/logs/*.log

set -euo pipefail

POPS=(60 120)
ITERS_LIST=(100 500)
SEEDS=(42 123 777)
OMP_THREADS=${OMP_NUM_THREADS:-12}
REPETICAO=1

DATASET_NAME="Victoire100"

STAMP=$(date +%Y%m%d_%H%M%S)
OUT_DIR="results/phase_a/${STAMP}"
LOG_DIR="${OUT_DIR}/logs"
MANIFEST="${OUT_DIR}/manifest.txt"
SUMMARY="${OUT_DIR}/summary.csv"
EQUIV="${OUT_DIR}/seq_omp_equivalence.csv"

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "NA")

mkdir -p "$LOG_DIR"

echo "== Fase A: compilando binarios =="
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
        echo "omp_num_threads_usado=${OMP_THREADS}"
        echo "load_average=$(uptime 2>/dev/null | sed 's/.*load average: //' || echo NA)"
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
    # Extrai tempo de kernel/PCIe do bloco "Perfil de tempo" (só existe no log do ACC_SYNC)
    local label=$1
    local file=$2
    grep -m1 -oP "${label}.*?:\s+\K[\d.]+(?=\s*s)" "$file" || echo "NA"
}

write_summary_header() {
    echo "timestamp,commit,phase,dataset,version,n_units,n_hours,n_vars,n_fireflies,iters,seed,repeticao,threads,alpha,beta0,gamma,cost,time_s,t_kernel_s,t_pcie_s,valid_ok,max_balance,max_pmin,max_pmax,max_ramp_up,max_ramp_down,nan,inf,observacoes,log_file" > "$SUMMARY"
}

write_equiv_header() {
    echo "phase,dataset,n_fireflies,iters,seed,seq_cost,omp_cost,abs_diff,equivalent" > "$EQUIV"
}

run_version() {
    local version=$1
    local bin=$2
    local n_fireflies=$3
    local seed=$4
    local iters=$5
    local threads=$6
    local log_file=$7

    echo "-> ${version} f=${n_fireflies} seed=${seed} iters=${iters}"

    local ts
    ts=$(date -Iseconds)

    if [[ "$version" == "OMP" ]]; then
        OMP_NUM_THREADS="$OMP_THREADS" "$bin" "$n_fireflies" "$seed" "$iters" > "$log_file" 2>&1
    else
        "$bin" "$n_fireflies" "$seed" "$iters" > "$log_file" 2>&1
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

    echo "${ts},${COMMIT},A,${DATASET_NAME},${version},${n_units},${n_hours},${n_vars},${n_fireflies},${iters},${seed},${REPETICAO},${threads},${alpha},${beta0},${gamma},${cost},${time_s},${t_kernel},${t_pcie},${valid},${max_balance},${max_pmin},${max_pmax},${max_ramp_up},${max_ramp_down},${nan_count},${inf_count},,${log_file}" >> "$SUMMARY"
}

write_summary_header
write_equiv_header

declare -A COSTS

for n_fireflies in "${POPS[@]}"; do
    for iters in "${ITERS_LIST[@]}"; do
        for seed in "${SEEDS[@]}"; do
            key="${n_fireflies}_${iters}_${seed}"

            seq_log="${LOG_DIR}/seq_f${n_fireflies}_s${seed}_i${iters}.log"
            omp_log="${LOG_DIR}/omp_f${n_fireflies}_s${seed}_i${iters}.log"
            acc_log="${LOG_DIR}/acc_sync_f${n_fireflies}_s${seed}_i${iters}.log"

            run_version "SEQ" "./bin/bin_seq_bench" "$n_fireflies" "$seed" "$iters" "1" "$seq_log"
            COSTS["${key}_SEQ"]=$(extract_field RESULT_COST "$seq_log")

            run_version "OMP" "./bin/bin_omp_bench" "$n_fireflies" "$seed" "$iters" "$OMP_THREADS" "$omp_log"
            COSTS["${key}_OMP"]=$(extract_field RESULT_COST "$omp_log")

            run_version "ACC_SYNC" "./bin/bin_acc_sync_bench" "$n_fireflies" "$seed" "$iters" "GPU" "$acc_log"
            COSTS["${key}_ACC_SYNC"]=$(extract_field RESULT_COST "$acc_log")

            seq_cost=${COSTS["${key}_SEQ"]}
            omp_cost=${COSTS["${key}_OMP"]}
            abs_diff=$(awk -v a="$seq_cost" -v b="$omp_cost" 'BEGIN { d=a-b; if (d<0) d=-d; printf "%.12f", d }')
            equivalent=$(awk -v d="$abs_diff" 'BEGIN { print (d <= 1e-9) ? 1 : 0 }')
            echo "A,${DATASET_NAME},${n_fireflies},${iters},${seed},${seq_cost},${omp_cost},${abs_diff},${equivalent}" >> "$EQUIV"
        done
    done
done

echo
echo "== Fase A concluida =="
echo "Manifesto de ambiente:  $MANIFEST"
echo "Resumo principal:       $SUMMARY"
echo "Equivalencia SEQ/OMP:   $EQUIV"
echo "Logs brutos:            $LOG_DIR"
