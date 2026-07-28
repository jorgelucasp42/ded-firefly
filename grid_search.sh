#!/usr/bin/env bash
# grid_search.sh — testa combinações de alpha/gamma com N seeds cada
# Uso: bash grid_search.sh [n_seeds]

set -e
cd "$(dirname "$0")"

N_SEEDS=${1:-10}
BIN=./bin/bin_seq

echo "Compilando..."
make seq -s
echo "OK. Rodando grid com $N_SEEDS seeds por configuracao..."
echo ""
printf "%-6s %-10s | %-16s %-16s %-16s | %s\n" \
       "alpha" "gamma" "min(\$)" "med(\$)" "max(\$)" "config"
echo "-------|-----------|------------------|------------------|------------------|---------------------------"

run_config() {
    local alpha=$1
    local gamma=$2
    local label=$3
    local vals=""

    for seed in $(seq 1 "$N_SEEDS"); do
        val=$("$BIN" "$seed" --alpha "$alpha" --gamma "$gamma" 2>/dev/null \
              | grep "Melhor custo encontrado:" \
              | awk '{print $NF}')
        if [ -n "$val" ]; then
            vals="$vals $val"
        fi
    done

    if [ -z "$(echo $vals | tr -d ' ')" ]; then
        printf "%-6s %-10s | ERRO: sem resultados\n" "$alpha" "$gamma"
        return
    fi

    python3 - << PYEOF
vals = [float(x) for x in "$vals".split()]
mn  = min(vals)
mx  = max(vals)
avg = sum(vals)/len(vals)
print(f"{$alpha:<6} {$gamma:<10} | {mn:<18.2f} {avg:<18.2f} {mx:<18.2f} | $label")
PYEOF
}

# BASELINE
run_config 0.15  1e-9   "ATUAL (baseline)"

# CANDIDATOS PRINCIPAIS
run_config 0.10  1e-6   "alpha-  gamma-mod"
run_config 0.05  1e-6   "alpha-- gamma-mod"
run_config 0.10  5e-6   "alpha-  gamma-forte"
run_config 0.05  5e-6   "alpha-- gamma-forte"
run_config 0.10  1e-5   "alpha-  gamma-mforte"
run_config 0.05  1e-5   "alpha-- gamma-mforte"

# EXPLORACAO EXTRA
run_config 0.20  1e-6   "alpha+  gamma-mod"
run_config 0.08  2e-6   "alpha-  gamma-2e6"
run_config 0.05  8e-6   "alpha-- gamma-8e6"

echo ""
echo "Concluido. Para rodar com 25 seeds: bash grid_search.sh 25"