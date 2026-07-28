# Protocolo de Testes — ded-firefly

Este documento descreve a metodologia usada para avaliar as implementações
do Firefly Algorithm aplicado ao Despacho Econômico Dinâmico (DED), para uso
na seção de Metodologia/Resultados do TCC.

## 1. Objetivo dos testes

Avaliar, para cada versão do algoritmo (sequencial, OpenMP, OpenACC/GPU):

1. **Desempenho**: tempo de execução e speedup em relação à versão sequencial.
2. **Qualidade da solução**: custo final encontrado (função objetivo do DED).
3. **Corretude**: aderência às restrições do problema (balanço de potência
   por hora e limites de rampa entre horas consecutivas).
4. **Escalabilidade**: como o speedup varia com o tamanho do enxame
   (nº de vagalumes) e, quando aplicável, com o nº de unidades geradoras.

## 2. Ambiente de execução

> Preencher com os dados exatos da máquina usada nas rodadas finais — os
> resultados já coletados em `bench_f*.txt` indicam `threads=12`, portanto
> uma CPU de 12 threads lógicas e uma GPU compatível com OpenACC (compilador
> `nvc`). Registrar aqui, para o TCC:

- CPU: modelo, nº de núcleos físicos/threads
- GPU: modelo, memória, driver CUDA
- Compiladores e versões: `gcc --version`, `nvc --version`
- Flags de compilação: `-O3 -Wall -Wextra -std=c11` (CPU),
  `-O3 -acc -Minfo=accel` (GPU) — ver `Makefile`
- Sistema operacional e versão do kernel
- `OMP_NUM_THREADS` usado (via `nproc` por padrão em `bench_compare.sh`)

## 3. Dados de entrada

- Dataset padrão dos benchmarks (`main_*_bench.c`): **100 unidades
  geradoras**, arquivos `data/units_victoire100.csv` e
  `data/load_24h_victoire100.csv` (perfil de carga de 24 horas).
- Datasets adicionais disponíveis para testes de escalabilidade por tamanho
  de sistema: 10, 200 e 500 unidades (`data/units_victoire{10,200,500}.csv`).
- **Atenção**: os coeficientes de custo de `units_victoire10.csv` e
  `units_victoire100.csv` foram corrigidos no commit `d5b6ce7` (colunas `a`
  e `c` estavam trocadas). Qualquer resultado gerado *antes* desse commit
  com esses dois arquivos deve ser descartado ou re-executado.

## 4. Parâmetros do algoritmo

- `alpha` (atração/randomização) e `gamma` (coeficiente de absorção de luz):
  valores padrão `alpha=0.25`, `gamma_val=3e-7` no `main_seq.c`; os
  benchmarks (`main_*_bench.c`) podem usar outro default — conferir no
  código antes de reportar no texto do TCC.
- A calibração desses hiperparâmetros é feita por `grid_search.sh`, que testa
  várias combinações de `alpha`/`gamma` com N seeds cada e reporta
  mínimo/média/máximo do custo. **Recomenda-se rodar o grid search antes da
  bateria final**, para não relatar resultados com hiperparâmetros
  desatualizados.

## 5. Desenho experimental

Dois experimentos complementares, ambos já parcialmente executados
(ver `bench_f*.txt` na raiz do repositório):

### 5.1 Experimento A — Escalabilidade por tamanho do enxame

Fixar `seed=42`, `iters=1000`, variar `n_fireflies` (60, 120, 160, 200, 240,
280, 320, 400, 480, ...) e registrar tempo/custo/speedup de SEQ, OMP e ACC.
Objetivo: mostrar como o speedup cresce (ou não) conforme aumenta o
paralelismo disponível no problema.

**Comando:**
```bash
make bench_seq bench_omp bench_acc
for f in 60 120 160 200 240 280 320 400 480; do
    ./bench_compare.sh "$f" 42 1000
done
```

### 5.2 Experimento B — Repetibilidade estatística

Fixar `n_fireflies=320`, `iters=1000`, variar a `seed` (ex.: 42, 123, 777,
1234, 9999, ...) e reportar média e desvio-padrão do custo e do tempo por
versão. Objetivo: mostrar que o ganho de desempenho é consistente e que a
qualidade da solução (custo) não piora de forma sistemática com a
paralelização — apenas varia dentro do ruído esperado de uma metaheurística
estocástica.

**Comando:**
```bash
for s in 42 123 777 1234 9999 5555 2024; do
    ./bench_compare.sh 320 "$s" 1000
done
```

**Recomendação**: usar pelo menos **10 seeds** por configuração reportada no
TCC (hoje há 5-6 no repositório) para ter uma base estatística mais sólida
(desvio-padrão, intervalo de confiança).

### 5.3 Experimento C (a fazer) — Escalabilidade por tamanho do sistema

Ainda não coberto pelos `bench_f*.txt` atuais (todos usam o dataset de 100
unidades). Recomenda-se rodar `bench_compare.sh` também com os datasets de
10, 200 e 500 unidades, o que exige criar variantes de
`main_*_bench.c` que apontem para esses arquivos (seguindo o padrão dos
`main_seq_100.c`, `main_seq_200.c` etc. já existentes, mas com saída no
formato `RESULT_COST=`/`RESULT_TIME=` esperado por `bench_compare.sh`).

## 6. Validação de corretude

Antes de reportar qualquer resultado de desempenho, rodar `bin_validate`
para confirmar:
- `Max |sumP - load|` ≈ 0 (balanço de potência respeitado em todas as horas)
- `Max violação de rampa (UR/DR)` ≈ 0 (limites de rampa respeitados)

Isso garante que ganhos de velocidade não vêm de soluções inválidas.

## 7. Registro dos resultados

- Saída bruta de cada rodada: `bench_f<F>_s<SEED>_i<ITERS>.txt` (gerado
  automaticamente por `bench_compare.sh`), commitado no repositório.
- Consolidação em tabela única: `scripts/consolidate_bench.py`, gera
  `docs/resultados_consolidados.csv` com uma linha por rodada.
- Recomenda-se rodar o script de consolidação após cada nova rodada de
  testes, e versionar o CSV atualizado junto com os `.txt` brutos.

```bash
python3 scripts/consolidate_bench.py
```

## 8. Do CSV para o texto do TCC

A partir de `docs/resultados_consolidados.csv`, sugerem-se as seguintes
figuras/tabelas para o capítulo de Resultados:

1. **Tabela** de speedup médio (OMP e ACC) por `n_fireflies`, com
   desvio-padrão quando houver repetições de seed.
2. **Gráfico de linhas**: speedup (eixo Y) vs. `n_fireflies` (eixo X), uma
   linha para OMP e outra para ACC — evidencia a escalabilidade.
3. **Boxplot ou tabela de dispersão** do `gap_*_pct` (diferença percentual de
   custo em relação ao SEQ) por versão, no experimento de repetibilidade
   (seção 5.2) — evidencia que a paralelização não compromete a qualidade da
   solução.
4. **Tabela de validação**: saída do `bin_validate` para cada tamanho de
   sistema testado, como evidência de corretude do modelo.

O script `scripts/consolidate_bench.py` cobre o item 1 diretamente; os
gráficos (2 e 3) podem ser gerados a partir do mesmo CSV com uma biblioteca
de plotagem (ex.: matplotlib) — ver seção "Próximos passos" no relatório de
resultados (`docs/resultados-tcc.md`).
