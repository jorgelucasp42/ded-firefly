#!/usr/bin/env bash
# Fase B — Escalabilidade por população
# Sistema: 100 unidades, 24 horas
#
# O tempo de execução do Firefly cresce em torno de pop^1.8 (o passo de
# atração compara cada vagalume com todos os outros a cada iteração —
# quase quadrático na população). Medido no piloto: SEQ(pop=60,iters=1000)
# = 10.7s, SEQ(pop=480,iters=1000) = 500.5s (~47x para 8x de população).
# As 3 populações maiores (320/400/480) respondem por ~2/3 do custo total
# da matriz. Por isso a amostragem abaixo é estratificada por custo, em vez
# de uniforme — desenho documentado em docs/PLANO_TESTES_TCC_DED_FIREFLY.md,
# seção "Fase B".
#
# Desenho:
#   - SEQ, OMP@12 threads e ACC_SYNC rodam em TODAS as 9 populações
#     (é o que responde ao objetivo principal: onde CPU paralela e GPU
#     se cruzam).
#   - A varredura completa de threads OMP (1,2,4,8,12) roda só nas
#     populações-âncora (60, 240, 480 — menor/meio/maior), para alimentar
#     a figura de eficiência OpenMP sem repetir o sweep nos 9 pontos.
#   - Sementes: 5 fixas nas 6 populações mais baratas (60-280), só 3 nas
#     3 mais caras (320-480) — a Fase D já cobre a estatística de 30
#     sementes na configuração final escolhida; aqui o objetivo é a
#     tendência, não a distribuição completa em cada ponto.
#
# Uso:
#   ./run_phase_b.sh          # matriz estratificada completa
#   ./run_phase_b.sh --pilot  # matriz minima, so pra validar o pipeline
#
# Saídas:
#   results/phase_b/<timestamp>/manifest.txt
#   results/phase_b/<timestamp>/summary.csv
#   results/phase_b/<timestamp>/logs/*.log

set -euo pipefail

PILOT=0
if [[ "${1:-}" == "--pilot" ]]; then
    PILOT=1
fi

ITERS=1000
POPS=(60 120 160 200 240 280 320 400 480)
HEAVY_POPS=(320 400 480)
ANCHOR_POPS=(60 240 480)
SEEDS_LIGHT=(42 123 777 1234 9999)
SEEDS_HEAVY=(42 123 777)
OMP_THREADS_FULL=(1 2 4 8 12)
OMP_THREADS_BASE=(12)

if [[ "$PILOT" -eq 1 ]]; then
    POPS=(60 480)
    SEEDS_LIGHT=(42)
    SEEDS_HEAVY=(42)
    OMP_THREADS_FULL=(1 12)
    echo "== MODO PILOTO (so 60 e 480, 1 semente, valida o pipeline) =="
fi

in_array() {
    local needle=$1; shift
    for x in "$@"; do [[ "$x" == "$needle" ]] && return 0; done
    return 1
}

REPETICAO=1
DATASET_NAME="Victoire100"

STAMP=$(date +%Y%m%d_%H%M%S)
SUFFIX=""
[[ "$PILOT" -eq 1 ]] && SUFFIX="_pilot"
OUT_DIR="results/phase_b/${STAMP}${SUFFIX}"
LOG_DIR="${OUT_DIR}/logs"
MANIFEST="${OUT_DIR}/manifest.txt"
SUMMARY="${OUT_DIR}/summary.csv"

COMMIT=$(git rev-parse --short HEAD 2>/dev/null || echo "NA")

mkdir -p "$LOG_DIR"

echo "== Fase B: compilando binarios =="
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
    local threads=$6
    local omp_env=$7
    local log_file=$8

    echo "-> ${version} f=${n_fireflies} seed=${seed} iters=${iters} threads=${threads}"

    local ts
    ts=$(date -Iseconds)

    if [[ -n "$omp_env" ]]; then
        OMP_NUM_THREADS="$omp_env" "$bin" "$n_fireflies" "$seed" "$iters" > "$log_file" 2>&1
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

    echo "${ts},${COMMIT},B,${DATASET_NAME},${version},${n_units},${n_hours},${n_vars},${n_fireflies},${iters},${seed},${REPETICAO},${threads},${alpha},${beta0},${gamma},${cost},${time_s},${t_kernel},${t_pcie},${valid},${max_balance},${max_pmin},${max_pmax},${max_ramp_up},${max_ramp_down},${nan_count},${inf_count},,${log_file}" >> "$SUMMARY"
}

write_summary_header

RUN_COUNT=0
T_START=$(date +%s)

for n_fireflies in "${POPS[@]}"; do
    if in_array "$n_fireflies" "${HEAVY_POPS[@]}"; then
        seeds=("${SEEDS_HEAVY[@]}")
    else
        seeds=("${SEEDS_LIGHT[@]}")
    fi

    if in_array "$n_fireflies" "${ANCHOR_POPS[@]}"; then
        threads_list=("${OMP_THREADS_FULL[@]}")
    else
        threads_list=("${OMP_THREADS_BASE[@]}")
    fi

    for seed in "${seeds[@]}"; do
        seq_log="${LOG_DIR}/seq_f${n_fireflies}_s${seed}_i${ITERS}.log"
        run_version "SEQ" "./bin/bin_seq_bench" "$n_fireflies" "$seed" "$ITERS" "1" "" "$seq_log"
        RUN_COUNT=$((RUN_COUNT+1))

        for threads in "${threads_list[@]}"; do
            omp_log="${LOG_DIR}/omp_t${threads}_f${n_fireflies}_s${seed}_i${ITERS}.log"
            run_version "OMP" "./bin/bin_omp_bench" "$n_fireflies" "$seed" "$ITERS" "$threads" "$threads" "$omp_log"
            RUN_COUNT=$((RUN_COUNT+1))
        done

        acc_log="${LOG_DIR}/acc_sync_f${n_fireflies}_s${seed}_i${ITERS}.log"
        run_version "ACC_SYNC" "./bin/bin_acc_sync_bench" "$n_fireflies" "$seed" "$ITERS" "GPU" "" "$acc_log"
        RUN_COUNT=$((RUN_COUNT+1))
    done
done

T_END=$(date +%s)
ELAPSED=$((T_END - T_START))

echo
echo "== Fase B ($([[ $PILOT -eq 1 ]] && echo PILOTO || echo "COMPLETA ESTRATIFICADA")) concluida =="
echo "Execucoes:              $RUN_COUNT"
echo "Tempo total:            ${ELAPSED}s (~$((ELAPSED/60)) min)"
echo "Manifesto de ambiente:  $MANIFEST"
echo "Resumo principal:       $SUMMARY"
echo "Logs brutos:            $LOG_DIR"

if [[ "$PILOT" -eq 1 ]]; then
    AVG=$(awk -v t="$ELAPSED" -v n="$RUN_COUNT" 'BEGIN { printf "%.2f", t/n }')
    echo
    echo "Piloto rodou $RUN_COUNT execucoes em ${ELAPSED}s (media ${AVG}s/execucao, dominado pelas de pop=480)."
    echo "A matriz estratificada completa tem 169 execucoes (vs. 630 da matriz uniforme original)."
    echo "Rode sem --pilot quando estiver pronto para a campanha completa."
fi
