# Plano de testes — DED com Firefly (SEQ, OpenMP e OpenACC)

## 1. Congelamento da versão experimental

Antes dos testes definitivos:

- corrigir e validar a equivalência algorítmica entre SEQ e OpenMP (mesma semente ⇒ mesmo resultado);
- garantir os mesmos parâmetros, dados, sementes e critério de parada em todas as versões;
- OpenACC não precisa reproduzir a mesma sequência de números aleatórios de SEQ/OpenMP: a lógica de
  geração/consumo de aleatoriedade usada por SEQ/OpenMP (stream de RNG independente por vagalume,
  ruído aplicado por vizinho atrator) não é portável para GPU de forma eficiente. Trata-se OpenACC
  como variante algorítmica distinta (ver regra de interpretação, seção 8), comparável em desempenho
  e em qualidade de solução (distribuição de custos), mas não em trajetória/valor exato;
- **versão OpenACC do estudo**: `bin_acc_sync_bench` (`src/firefly/firefly_openacc_sync.c`, alvo
  `make bench_acc_sync`) é a única versão OpenACC usada em todas as fases (A–E), inclusive na validação
  de corretude — o gasto computacional dela foi deliberadamente concentrado em GPU (kernels de
  atração/reparo/custo), o que exige essa reestruturação do algoritmo em relação ao SEQ/OpenMP.
  `bin_acc` (`src/firefly/firefly_openacc.c`) não é usado no estudo: roda o laço de movimento/reparo
  em CPU (serial, igual ao SEQ) só com o cálculo de custo em GPU, e não representa a versão
  GPU-otimizada que está sendo avaliada;
- registrar o hash do commit Git;
- manter o repositório sem alterações não versionadas;
- salvar o relatório de compilação do OpenACC (`-Minfo=accel`).

## 2. Validação funcional obrigatória

Para cada versão e configuração:

- conferir balanço de potência por hora;
- conferir limites Pmin/Pmax;
- conferir rampas UR/DR;
- registrar a maior violação encontrada;
- verificar ausência de NaN/Inf;
- executar SEQ e OpenMP com a mesma semente e confirmar resultados idênticos ou explicar diferenças;
- para OpenACC, validar apenas corretude (restrições, ausência de NaN/Inf) e qualidade da solução por
  distribuição de custo (Fase D), não equivalência numérica com SEQ/OpenMP.

## 3. Fases experimentais

### Fase A — Correção e reprodutibilidade

Sistema: 100 unidades, 24 horas.

- populações: 60 e 120;
- iterações: 100 e 500;
- sementes: 42, 123, 777;
- versões: SEQ, OpenMP e OpenACC (`bin_acc_sync_bench`).

Objetivo: verificar correção (restrições, NaN/Inf) em todas as versões e repetibilidade/equivalência
entre SEQ e OpenMP (mesma semente ⇒ mesmo resultado). OpenACC (`bin_acc_sync_bench`) é avaliado apenas
quanto à corretude nesta fase, não à equivalência numérica — ver seção 1.

### Fase B — Escalabilidade por população

Sistema: 100 unidades, 24 horas.

O tempo de execução do Firefly cresce em torno de população^1,8 (o passo de atração compara cada
vagalume com todos os outros a cada iteração — quase quadrático). Medido em piloto real:
SEQ(pop=60, iters=1000) = 10,7s; SEQ(pop=480, iters=1000) = 500,5s (~47x de tempo para 8x de
população). As 3 populações maiores (320/400/480) respondem sozinhas por ~2/3 do custo
computacional da matriz completa. Por isso a amostragem é **estratificada por custo**, não uniforme:

- populações: 60, 120, 160, 200, 240, 280, 320, 400 e 480;
- iterações: 1.000;
- **SEQ, OpenMP@12 threads e OpenACC** (`bin_acc_sync_bench`; não usar `bin_acc` — ver seção 1) rodam
  em **todas as 9 populações** — é o que responde ao objetivo principal;
- **varredura completa de threads OpenMP** (1, 2, 4, 8, 12) roda só nas populações-âncora **60, 240
  e 480** (menor/meio/maior), alimentando a figura de eficiência OpenMP (seção 7, item 5) sem repetir
  o sweep nos 9 pontos;
- sementes: **5 fixas** (42, 123, 777, 1234, 9999) nas 6 populações mais baratas (60-280); **3 fixas**
  (42, 123, 777) nas 3 mais caras (320-400-480) — a Fase D já cobre a estatística de 30 sementes na
  configuração final escolhida, então aqui o objetivo é a tendência de escalabilidade, não a
  distribuição completa em cada ponto;
- total: 169 execuções (vs. 630 de uma matriz uniforme 9×10×7) — ver `run_phase_b.sh`.

Objetivo: (1) identificar o ponto em que CPU paralela (OpenMP@12) e GPU (OpenACC) amortizam seus
custos frente ao SEQ; (2) gerar o dataset de escalabilidade por número de threads OpenMP (1→12) nas
populações-âncora, usado na figura de speedup/eficiência (seção 7, item 5).

### Fase C — Escalabilidade pelo tamanho do sistema

- sistemas: 10, 100, 200 e 500 unidades;
- **população principal: 320** — definido a partir da Fase B (`results/phase_b/20260810_141423/`):
  o cruzamento CPU↔GPU acontece entre pop=200 (empate: 16,4s OMP@12 vs 16,5s ACC) e pop=240 (GPU já
  vence: 19,9s vs 25,1s); 320 fica folgadamente do lado favorável à GPU (ACC ~1,34x mais rápido que
  OMP@12) sem ser o ponto computacionalmente mais caro (480);
- iterações: 1.000;
- sementes (10, fixas — mesmo conjunto da Fase B): 42, 123, 777, 1234, 9999, 2024, 5555, 8080, 271, 63;
- versões: SEQ, OpenMP@12 threads e OpenACC — os três binários `_bench` (`bin_seq_bench`,
  `bin_omp_bench`, `bin_acc_sync_bench`) foram generalizados para aceitar o tamanho do sistema como
  4º parâmetro opcional (`<n_fireflies> <seed> <max_iters> [10|100|200|500]`, default 100 — mantém
  `run_phase_a.sh`/`run_phase_b.sh` funcionando sem alteração);
- script: `run_phase_c.sh` (mesmo padrão de manifesto/CSV das Fases A e B), com `--pilot` (só 10 e
  500 unidades, 1 semente) para validar a hipótese de escala antes da matriz completa (120 execuções).

Objetivo: avaliar escalabilidade em função do número de variáveis do DED. Diferente da Fase B (custo
~pop^1,8, quase quadrático, população variando), aqui a população fica fixa em 320 e só o número de
variáveis cresce — pela estrutura do algoritmo (atração é O(pop²×n_vars); reparo/custo são
O(pop×n_vars)), a expectativa é crescimento ~linear em n_vars, ainda não confirmado empiricamente.

### Fase D — Qualidade da solução

- configuração escolhida nas fases anteriores;
- 30 sementes independentes;
- registrar melhor custo, média, mediana, desvio-padrão, pior custo e coeficiente de variação;
- registrar taxa de soluções factíveis;
- comparar distribuições, não apenas a melhor execução.

### Fase E — Desempenho final

- realizar 1 execução de aquecimento que não entra nas estatísticas;
- realizar pelo menos 10 repetições cronometradas por configuração;
- reportar mediana do tempo e intervalo interquartil; média e desvio-padrão podem ser apresentados como complemento;
- calcular speedup `T_seq/T_par` e eficiência OpenMP `speedup/n_threads`;
- separar, no OpenACC, tempo total, kernels, transferências e processamento na CPU.

## 4. Condições de execução

Registrar em cada campanha:

- data e hora;
- hash do commit;
- sistema operacional e kernel;
- modelo da CPU e quantidade de núcleos/threads;
- modelo da GPU e memória;
- memória RAM;
- versões de GCC, NVIDIA HPC SDK/NVC e driver NVIDIA;
- flags de compilação;
- `OMP_NUM_THREADS`, afinidade e política de escalonamento;
- estado energético da máquina e processos concorrentes relevantes.

Evitar misturar resultados de commits, compiladores ou configurações diferentes na mesma tabela.

## 5. Arquivos de saída

Estrutura recomendada:

```text
experiments/
  manifests/
    ambiente_YYYYMMDD.txt
  raw/
    <commit>/<dataset>/<versao>/
  processed/
    resultados_consolidados.csv
  logs/
    compilacao_acc_<commit>.log
  figures/
  scripts/
```

Cada execução deve gerar uma linha CSV, sem sobrescrever resultados anteriores.

## 6. Colunas mínimas do CSV

- timestamp;
- commit;
- versão;
- dataset;
- número de unidades e horas;
- número de variáveis;
- população;
- iterações;
- semente;
- repetição;
- threads;
- alpha, beta0 e gamma;
- custo final;
- tempo total;
- factível;
- maior erro de balanço;
- maior violação de rampa;
- tempo de kernels;
- tempo de transferências;
- observações.

## 7. Tabelas e figuras para o TCC

1. Caracterização do ambiente computacional.
2. Parâmetros do Firefly e critérios experimentais.
3. Validação das restrições e equivalência funcional.
4. Tempo por versão e população.
5. Speedup e eficiência OpenMP.
6. Tempo por tamanho do sistema.
7. Estatísticas do custo em 30 sementes.
8. Perfil do OpenACC: CPU, PCIe e kernels.
9. Curva de convergência: melhor custo por iteração.

## 8. Regra de interpretação

Não atribuir diferenças de custo exclusivamente ao paralelismo quando as versões executarem regras de movimento, geração aleatória ou ordem de atualização diferentes. Nesse caso, tratar a implementação como variante algorítmica distinta e analisá-la separadamente.
