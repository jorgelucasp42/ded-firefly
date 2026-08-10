#!/usr/bin/env bash
# bench_compare.sh — compara SEQ vs OMP vs ACC para DED 100u
# Uso: ./bench_compare.sh [n_fireflies] [seed] [max_iters]

set -euo pipefail

F=${1:-60}
SEED=${2:-42}
ITERS=${3:-1000}

SEQ_BIN="./bin/bin_seq_bench"
OMP_BIN="./bin/bin_omp_bench"
ACC_BIN="./bin/bin_acc_sync_bench"

for bin in "$SEQ_BIN" "$OMP_BIN" "$ACC_BIN"; do
    if [[ ! -x "$bin" ]]; then
        echo "ERRO: '$bin' nao encontrado."
        echo "Rode: make bench_seq bench_omp bench_acc_sync"
        exit 1
    fi
done

export OMP_NUM_THREADS=${OMP_NUM_THREADS:-$(nproc)}

sep()  { printf '%.0s-' {1..66}; echo; }
sep2() { printf '%.0s=' {1..66}; echo; }

run_bin() {
    local label=$1 bin=$2
    echo ""; sep
    printf "> %s  (f=%s  seed=%s  iters=%s)\n" "$label" "$F" "$SEED" "$ITERS"
    sep

    local tmpfile; tmpfile=$(mktemp)
    "$bin" "$F" "$SEED" "$ITERS" 2>&1 | tee "$tmpfile"

    local cost time_s valid max_balance max_pmin max_pmax max_ramp_up max_ramp_down nan_count inf_count
    cost=$(grep  -oP 'RESULT_COST=\K[\d.]+' "$tmpfile" || echo "0")
    time_s=$(grep -oP 'RESULT_TIME=\K[\d.]+' "$tmpfile" || echo "0")
    valid=$(grep -oP 'RESULT_VALID_OK=\K[01]' "$tmpfile" || echo "0")
    max_balance=$(grep -oP 'RESULT_MAX_BALANCE=\K[\d.]+' "$tmpfile" || echo "0")
    max_pmin=$(grep -oP 'RESULT_MAX_PMIN=\K[\d.]+' "$tmpfile" || echo "0")
    max_pmax=$(grep -oP 'RESULT_MAX_PMAX=\K[\d.]+' "$tmpfile" || echo "0")
    max_ramp_up=$(grep -oP 'RESULT_MAX_RAMP_UP=\K[\d.]+' "$tmpfile" || echo "0")
    max_ramp_down=$(grep -oP 'RESULT_MAX_RAMP_DOWN=\K[\d.]+' "$tmpfile" || echo "0")
    nan_count=$(grep -oP 'RESULT_NAN=\K[0-9]+' "$tmpfile" || echo "0")
    inf_count=$(grep -oP 'RESULT_INF=\K[0-9]+' "$tmpfile" || echo "0")
    rm -f "$tmpfile"

    echo "___PARSED___ ${cost} ${time_s} ${valid} ${max_balance} ${max_pmin} ${max_pmax} ${max_ramp_up} ${max_ramp_down} ${nan_count} ${inf_count}"
}

tmp_seq=$(mktemp)
tmp_omp=$(mktemp)
tmp_acc=$(mktemp)

run_bin "SEQ (1 thread)"                    "$SEQ_BIN" | tee "$tmp_seq"
run_bin "OMP (${OMP_NUM_THREADS} threads)"  "$OMP_BIN" | tee "$tmp_omp"
run_bin "ACC (GPU)"                         "$ACC_BIN" | tee "$tmp_acc"

seq_cost=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $2}')
seq_time=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $3}')
seq_valid=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $4}')
seq_balance=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $5}')
seq_pmin=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $6}')
seq_pmax=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $7}')
seq_ramp_up=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $8}')
seq_ramp_down=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $9}')
seq_nan=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $10}')
seq_inf=$(grep "^___PARSED___" "$tmp_seq" | awk '{print $11}')
omp_cost=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $2}')
omp_time=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $3}')
omp_valid=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $4}')
omp_balance=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $5}')
omp_pmin=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $6}')
omp_pmax=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $7}')
omp_ramp_up=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $8}')
omp_ramp_down=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $9}')
omp_nan=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $10}')
omp_inf=$(grep "^___PARSED___" "$tmp_omp" | awk '{print $11}')
acc_cost=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $2}')
acc_time=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $3}')
acc_valid=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $4}')
acc_balance=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $5}')
acc_pmin=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $6}')
acc_pmax=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $7}')
acc_ramp_up=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $8}')
acc_ramp_down=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $9}')
acc_nan=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $10}')
acc_inf=$(grep "^___PARSED___" "$tmp_acc" | awk '{print $11}')
rm -f "$tmp_seq" "$tmp_omp" "$tmp_acc"

sp_omp=$(awk "BEGIN { if ($omp_time+0>0) printf \"%.2f\", $seq_time/$omp_time; else print \"N/A\" }")
sp_acc=$(awk "BEGIN { if ($acc_time+0>0) printf \"%.2f\", $seq_time/$acc_time; else print \"N/A\" }")
gp_omp=$(awk "BEGIN { if ($seq_cost+0>0) printf \"%.4f\", ($omp_cost-$seq_cost)/$seq_cost*100; else print \"N/A\" }")
gp_acc=$(awk "BEGIN { if ($seq_cost+0>0) printf \"%.4f\", ($acc_cost-$seq_cost)/$seq_cost*100; else print \"N/A\" }")

echo ""; sep2
printf "  %-26s  %12s  %12s  %12s\n" "Metrica"      "SEQ"      "OMP"      "ACC (GPU)"
sep2
printf "  %-26s  %12s  %12s  %12s\n" "Vagalumes"    "$F"       "$F"       "$F"
printf "  %-26s  %12s  %12s  %12s\n" "Iteracoes"    "$ITERS"   "$ITERS"   "$ITERS"
printf "  %-26s  %12s  %12s  %12s\n" "Seed"         "$SEED"    "$SEED"    "$SEED"
printf "  %-26s  %12s  %12s  %12s\n" "Threads/GPU"  "1"        "${OMP_NUM_THREADS}" "GPU"
sep
printf "  %-26s  %12s  %12s  %12s\n" "Melhor custo" "$seq_cost" "$omp_cost" "$acc_cost"
printf "  %-26s  %12s  %12s  %12s\n" "Tempo (s)"    "$seq_time" "$omp_time" "$acc_time"
printf "  %-26s  %12s  %12s  %12s\n" "Validacao OK" "$seq_valid" "$omp_valid" "$acc_valid"
sep
printf "  %-26s  %12s  %12s  %12s\n" "Speedup vs SEQ" "1.00x"  "${sp_omp}x" "${sp_acc}x"
printf "  %-26s  %12s  %12s  %12s\n" "Gap custo vs SEQ" "ref"  "${gp_omp}%" "${gp_acc}%"
sep
printf "  %-26s  %12s  %12s  %12s\n" "Max balance MW" "$seq_balance" "$omp_balance" "$acc_balance"
printf "  %-26s  %12s  %12s  %12s\n" "Max Pmin MW" "$seq_pmin" "$omp_pmin" "$acc_pmin"
printf "  %-26s  %12s  %12s  %12s\n" "Max Pmax MW" "$seq_pmax" "$omp_pmax" "$acc_pmax"
printf "  %-26s  %12s  %12s  %12s\n" "Max ramp up MW" "$seq_ramp_up" "$omp_ramp_up" "$acc_ramp_up"
printf "  %-26s  %12s  %12s  %12s\n" "Max ramp down MW" "$seq_ramp_down" "$omp_ramp_down" "$acc_ramp_down"
printf "  %-26s  %12s  %12s  %12s\n" "NaN/Inf" "${seq_nan}/${seq_inf}" "${omp_nan}/${omp_inf}" "${acc_nan}/${acc_inf}"
sep2; echo ""

awk -v so="$sp_omp" -v sa="$sp_acc" -v go="$gp_omp" -v ga="$gp_acc" \
    -v nt="$OMP_NUM_THREADS" 'BEGIN {
    so=so+0; sa=sa+0; go=go+0; ga=ga+0
    printf "  Speedup: OMP %.2fx  |  ACC %.2fx  (ACC/OMP: %.2fx)\n", so, sa, sa/so
    if (ga < go)
        printf "  Qualidade: ACC encontrou o melhor custo (%.4f%% abaixo do SEQ).\n", -ga
    else if (go < 0)
        printf "  Qualidade: OMP encontrou o melhor custo (%.4f%% abaixo do SEQ).\n", -go
    else
        printf "  Qualidade: SEQ encontrou o melhor custo nesta rodada.\n"
}'

OUT="bench_f${F}_s${SEED}_i${ITERS}.txt"
{
    echo "f=$F seed=$SEED iters=$ITERS threads=$OMP_NUM_THREADS"
    echo "seq_cost=$seq_cost  seq_time=$seq_time"
    echo "omp_cost=$omp_cost  omp_time=$omp_time"
    echo "acc_cost=$acc_cost  acc_time=$acc_time"
    echo "speedup_omp=${sp_omp}x  speedup_acc=${sp_acc}x"
    echo "gap_omp=${gp_omp}%  gap_acc=${gp_acc}%"
    echo "seq_valid=$seq_valid  seq_max_balance=$seq_balance  seq_max_pmin=$seq_pmin  seq_max_pmax=$seq_pmax  seq_max_ramp_up=$seq_ramp_up  seq_max_ramp_down=$seq_ramp_down  seq_nan=$seq_nan  seq_inf=$seq_inf"
    echo "omp_valid=$omp_valid  omp_max_balance=$omp_balance  omp_max_pmin=$omp_pmin  omp_max_pmax=$omp_pmax  omp_max_ramp_up=$omp_ramp_up  omp_max_ramp_down=$omp_ramp_down  omp_nan=$omp_nan  omp_inf=$omp_inf"
    echo "acc_valid=$acc_valid  acc_max_balance=$acc_balance  acc_max_pmin=$acc_pmin  acc_max_pmax=$acc_pmax  acc_max_ramp_up=$acc_ramp_up  acc_max_ramp_down=$acc_ramp_down  acc_nan=$acc_nan  acc_inf=$acc_inf"
} > "$OUT"
echo "  Salvo em: $OUT"; echo ""
