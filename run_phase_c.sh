#!/usr/bin/env bash
# Fase C — Escalabilidade pelo tamanho do sistema
# População fixa (320, definida pela Fase B), variando o número de
# unidades geradoras (10, 100, 200, 500) — ou seja, o número de variáveis
# do DED (n_vars = unidades x 24 horas), não o tamanho do enxame.
#
# Objetivo: avaliar escalabilidade em função do número de variáveis do DED.
#
# Diferente da Fase B (custo ~pop^1,8, quase quadrático), aqui a população
# fica fixa e só o numero de variaveis cresce — pela estrutura do algoritmo
# (passo de atração é O(pop² x n_vars), reparo/custo são O(pop x n_vars)),
# o crescimento esperado é ~linear em n_vars. Isso ainda não foi medido
# empiricamente — use --pilot antes de rodar a matriz completa pra
# confirmar essa hipótese com os extremos (10 e 500 unidades).
#
# Uso:
#   ./run_phase_c.sh          # matriz completa (4 sistemas x 10 sementes x 3 versoes = 120 execucoes)
#   ./run_phase_c.sh --pilot  # so 10 e 500 unidades, 1 semente, pra validar o pipeline e a hipotese de escala
#
# Saídas:
#   results/phase_c/<timestamp>/manifest.txt
#   results/phase_c/<timestamp>/summary.csv
#   results/phase_c/<timestamp>/logs/*.log

set -euo pipefail

PILOT=0
if [[ "${1:-}" == "--pilot" ]]; then
    PILOT=1
fi

POP=320
ITERS=1000
SYSTEMS=(10 100 200 500)
SEEDS=(42 123 777 1234 9999 2024 5555 8080 271 63)
OMP_THREADS=12

if [[ "$PILOT" -eq 1 ]]; then
    SYSTEMS=(10 500)
    SEEDS=(42)
    echo "== MODO PILOTO (so 10 e 500 unidades, 1 semente, valida pipeline e hipotese de escala linear) =="
fi

REPETICAO=1

STAMP=$(date +%Y%m%d_%H%M%S)
SUFFIX=""
[[ "$PILOT" -eq 1 ]] && SUFFIX="_pilot"
OUT_DIR="results/phase_c/${STAMP}${SUFFIX}"
LOG_DIR="${OUT_DIR}/logs"
MANIFEST="${OUT_DIR}/manifest.txt"
SUMMARY="${OUT_DIR}/summary.csv"

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "NA")

mkdir -p "$LOG_DIR"

echo "== Fase C: compilando binarios =="
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
    local n_fireflies=$3
    local seed=$4
    local iters=$5
    local system=$6
    local threads=$7
    local omp_env=$8
    local log_file=$9

    echo "-> ${version} sistema=${system} f=${n_fireflies} seed=${seed} iters=${iters}"

    local ts
    ts=$(date -Iseconds)

    if [[ -n "$omp_env" ]]; then
        OMP_NUM_THREADS="$omp_env" "$bin" "$n_fireflies" "$seed" "$iters" "$system" > "$log_file" 2>&1
    else
        "$bin" "$n_fireflies" "$seed" "$iters" "$system" > "$log_file" 2>&1
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

    echo "${ts},${COMMIT},C,Victoire${system},${version},${n_units},${n_hours},${n_vars},${n_fireflies},${iters},${seed},${REPETICAO},${threads},${alpha},${beta0},${gamma},${cost},${time_s},${t_kernel},${t_pcie},${valid},${max_balance},${max_pmin},${max_pmax},${max_ramp_up},${max_ramp_down},${nan_count},${inf_count},,${log_file}" >> "$SUMMARY"
}

write_summary_header

RUN_COUNT=0
T_START=$(date +%s)

for system in "${SYSTEMS[@]}"; do
    for seed in "${SEEDS[@]}"; do
        seq_log="${LOG_DIR}/seq_sys${system}_s${seed}_i${ITERS}.log"
        run_version "SEQ" "./bin/bin_seq_bench" "$POP" "$seed" "$ITERS" "$system" "1" "" "$seq_log"
        RUN_COUNT=$((RUN_COUNT+1))

        omp_log="${LOG_DIR}/omp_t${OMP_THREADS}_sys${system}_s${seed}_i${ITERS}.log"
        run_version "OMP" "./bin/bin_omp_bench" "$POP" "$seed" "$ITERS" "$system" "$OMP_THREADS" "$OMP_THREADS" "$omp_log"
        RUN_COUNT=$((RUN_COUNT+1))

        acc_log="${LOG_DIR}/acc_sync_sys${system}_s${seed}_i${ITERS}.log"
        run_version "ACC_SYNC" "./bin/bin_acc_sync_bench" "$POP" "$seed" "$ITERS" "$system" "GPU" "" "$acc_log"
        RUN_COUNT=$((RUN_COUNT+1))
    done
done

T_END=$(date +%s)
ELAPSED=$((T_END - T_START))

echo
echo "== Fase C ($([[ $PILOT -eq 1 ]] && echo PILOTO || echo COMPLETA)) concluida =="
echo "Execucoes:              $RUN_COUNT"
echo "Tempo total:            ${ELAPSED}s (~$((ELAPSED/60)) min)"
echo "Manifesto de ambiente:  $MANIFEST"
echo "Resumo principal:       $SUMMARY"
echo "Logs brutos:            $LOG_DIR"

if [[ "$PILOT" -eq 1 ]]; then
    echo
    echo "Compare os tempos SEQ/OMP/ACC entre sistema=10 e sistema=500 em '$SUMMARY'."
    echo "Se o crescimento for proximo de linear (~50x de tempo para 50x de unidades,"
    echo "10->500), a matriz completa (120 execucoes, 4 sistemas x 10 sementes x 3 versoes)"
    echo "deve ficar bem mais rapida que a Fase B. Se for super-linear, considere"
    echo "estratificar sementes como foi feito na Fase B antes de rodar sem --pilot."
fi
