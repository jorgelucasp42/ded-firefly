# Achados e conclusões das Fases A, B e C

Este documento consolida os principais achados experimentais das Fases A, B e C do plano de testes do projeto `ded-firefly`.

As versões oficiais consideradas no estudo são:

- `SEQ`: implementação sequencial;
- `OMP`: implementação OpenMP;
- `ACC_SYNC`: implementação OpenACC síncrona, usada como variante GPU oficial.

A equivalência numérica é exigida apenas entre `SEQ` e `OMP`. A versão `ACC_SYNC` é tratada como variante algorítmica distinta, avaliada por corretude funcional, qualidade da solução e desempenho.

## Ambiente computacional

Todas as campanhas rodaram na mesma máquina (ver `manifest.txt` dentro de cada pasta de resultado):

- CPU: Intel Core i5-12450HX (8 núcleos físicos / 12 threads lógicos).
- GPU: NVIDIA GeForce RTX 3050 6GB Laptop GPU, driver 595.84.
- Compiladores: `gcc` 13.3.0 (Ubuntu 13.3.0-6ubuntu2~24.04.1) e `nvc` 25.11-0.
- Flags: `-O3 -Wall -Wextra -std=c11` (CPU) e `-O3 -acc -Minfo=accel` (GPU).

As três fases rodaram em commits diferentes do repositório (Fase A: `f22cf2b`, Fase B: `fbd0827`,
Fase C: `6eca924`), refletindo a evolução da infraestrutura de automação (scripts, campos
`RESULT_*`, organização do Makefile) ao longo do trabalho. Isso não compromete a comparação entre
fases: os arquivos-núcleo do algoritmo (`firefly_seq.c`, `firefly_omp.c`, `firefly_openacc_sync.c`)
permaneceram idênticos em todos esses commits — só a camada de automação em volta mudou.

## Fase A — Correção e reprodutibilidade

Arquivo principal:

```text
results/phase_a/20260810_122145/summary.csv
```

Arquivo de equivalência:

```text
results/phase_a/20260810_122145/seq_omp_equivalence.csv
```

### Configuração

- Sistema: 100 unidades, 24 horas.
- Populações: 60 e 120 vagalumes.
- Iterações: 100 e 500.
- Sementes: 42, 123 e 777.
- Versões: `SEQ`, `OMP` e `ACC_SYNC`.
- Total: 36 execuções.

### Achados

- 36/36 execuções válidas.
- Nenhuma ocorrência de `NaN` ou `Inf`.
- Nenhuma violação de `Pmin`, `Pmax`, rampa de subida (`UR`) ou rampa de descida (`DR`).
- Maior erro de balanço: aproximadamente `3.43611e-07 MW`.
- `SEQ` e `OMP` foram equivalentes em 12/12 comparações, com diferença absoluta zero.
- `ACC_SYNC` também produziu soluções factíveis em todos os casos, mas não foi avaliado por equivalência numérica com `SEQ`/`OMP`.

### Conclusão

A Fase A confirmou que as três implementações oficiais produzem soluções factíveis para o sistema de 100 unidades. A equivalência exata entre `SEQ` e `OMP` mostrou que a paralelização OpenMP preserva a trajetória do algoritmo sequencial. A versão `ACC_SYNC`, por sua vez, foi validada funcionalmente como variante GPU.

## Fase B — Escalabilidade por população

Arquivo principal:

```text
results/phase_b/20260810_141423/summary.csv
```

### Configuração

- Sistema: 100 unidades, 24 horas.
- Iterações: 1000.
- Populações avaliadas: 60, 120, 160, 200, 240, 280, 320, 400 e 480.
- Versões: `SEQ`, `OMP` e `ACC_SYNC`.
- Para OpenMP, foi analisado especialmente o desempenho com 12 threads.
- Total: 169 execuções (amostragem estratificada por custo, não uma matriz uniforme 9×10×7 — que
  seria de 630 execuções). O tempo do Firefly cresce em torno de população^1,85 (quase quadrático),
  então: `SEQ`, `OMP@12` e `ACC_SYNC` rodam nas 9 populações; a varredura completa de threads
  (1, 2, 4, 8, 12) roda só nas populações-âncora 60, 240 e 480 (menor/meio/maior); e usam-se 5
  sementes fixas nas 6 populações mais baratas (60–280) e 3 nas 3 mais caras (320–480), já que a
  Fase D cobre separadamente a estatística de 30 sementes na configuração final.

### Achados de corretude

- 169/169 execuções válidas.
- Nenhuma ocorrência de `NaN` ou `Inf`.
- Nenhuma violação de limites ou rampas.
- Maior erro de balanço: aproximadamente `2.2e-11 MW`.
- `SEQ` e `OMP` foram equivalentes em 91/91 comparações.

### Achados de desempenho

Médias observadas por população (todos os 9 pontos testados):

| População | SEQ | OMP@12 | ACC_SYNC | OMP@12 / ACC_SYNC |
|---:|---:|---:|---:|---:|
| 60 | 10.22 s | 4.51 s | 5.33 s | 0.85x |
| 120 | 34.10 s | 7.24 s | 9.60 s | 0.75x |
| 160 | 57.75 s | 11.50 s | 12.45 s | 0.92x |
| 200 | 87.78 s | 16.43 s | 16.46 s | 1.00x |
| 240 | 124.63 s | 25.15 s | 19.91 s | 1.26x |
| 280 | 167.71 s | 31.11 s | 24.72 s | 1.26x |
| 320 | 217.39 s | 40.08 s | 29.89 s | 1.34x |
| 400 | 336.90 s | 62.87 s | 43.35 s | 1.45x |
| 480 | 482.72 s | 92.56 s | 62.62 s | 1.48x |

O ponto de cruzamento entre CPU paralela e GPU ocorreu entre 200 e 240 vagalumes — em pop=200 os
tempos médios de `OMP@12` e `ACC_SYNC` ficaram praticamente empatados (16,43s vs 16,46s), e essa
troca de posição foi consistente nas 5 sementes individuais em ambas as populações, não só na
média. Abaixo desse intervalo, OpenMP foi mais rápido; acima dele, `ACC_SYNC` passou a vencer, com
vantagem crescente até 1,48x em 480 vagalumes.

### Fração de tempo em GPU

| População | Kernel GPU / tempo ACC_SYNC | PCIe / tempo ACC_SYNC |
|---:|---:|---:|
| 60 | 89.57% | 4.09% |
| 120 | 91.21% | 4.22% |
| 160 | 90.62% | 5.43% |
| 200 | 91.53% | 5.08% |
| 240 | 91.81% | 5.08% |
| 280 | 92.47% | 4.78% |
| 320 | 92.92% | 4.51% |
| 400 | 94.23% | 3.69% |
| 480 | 95.22% | 2.94% |

A fração de tempo dominada por kernels GPU cresce com a população (89,6% → 95,2%), o mesmo padrão
qualitativo observado na Fase C em função do tamanho do sistema — evidência de que o overhead
relativo de CPU/transferência diminui conforme a carga de trabalho da GPU aumenta.

### Eficiência por número de threads OpenMP

Medida nas populações-âncora (60, 240 e 480), com sweep completo de 1 a 12 threads:

| Threads | pop=60 (tempo / eficiência) | pop=240 (tempo / eficiência) | pop=480 (tempo / eficiência) |
|---:|---:|---:|---:|
| 1 | 10.30 s / 99.3% | 125.27 s / 99.5% | 489.80 s / 98.6% |
| 2 | 5.65 s / 90.4% | 65.26 s / 95.5% | 250.77 s / 96.2% |
| 4 | 3.43 s / 74.5% | 35.53 s / 87.7% | 138.18 s / 87.3% |
| 8 | 4.14 s / 30.9% | 31.52 s / 49.4% | 111.22 s / 54.3% |
| 12 | 4.51 s / 18.9% | 25.15 s / 41.3% | 92.56 s / 43.5% |

**Achado relevante**: em população pequena (60), 4 threads é o ponto ótimo em tempo absoluto — usar
8 ou 12 threads piora o tempo (3,43s em 4 threads vs 4,51s em 12 threads), porque o overhead de
paralelização passa a superar o ganho, com a eficiência caindo de 74,5% para 18,9%. Em populações
maiores (240, 480), a eficiência se degrada de forma mais suave, mas nunca ultrapassa ~44% em 12
threads — reforça que o OpenMP tem um teto de escalabilidade bem mais baixo que o OpenACC.

### Achados de qualidade

O `ACC_SYNC` encontrou custos médios menores que `SEQ`/`OMP` em todas as populações. Em média por população, o gap ficou aproximadamente entre `-0.6%` e `-0.8%`.

### Conclusão

A Fase B mostrou que a GPU precisa de carga suficiente para amortizar seus custos. Em populações pequenas, o OpenMP é mais eficiente. A partir de aproximadamente 240 vagalumes, a versão `ACC_SYNC` passa a superar o OpenMP, com vantagem crescente até 480 vagalumes. Esse resultado justificou a escolha de 320 vagalumes para a Fase C. Além disso, o sweep de threads mostrou que o próprio OpenMP tem escalabilidade limitada — eficiência nunca passa de ~44% em 12 threads, e em populações pequenas o ponto ótimo é 4 threads, não 12 — reforçando que o teto de desempenho da CPU paralela é estrutural, não uma questão de configuração.

## Fase C — Escalabilidade por tamanho do sistema

Arquivo principal:

```text
results/phase_c/20260810_183722/summary.csv
```

### Configuração

- População fixa: 320 vagalumes.
- Iterações: 1000.
- Sistemas avaliados: 10, 100, 200 e 500 unidades.
- Variáveis de decisão: 240, 2400, 4800 e 12000.
- Sementes: 10 sementes por sistema.
- Versões: `SEQ`, `OMP@12` e `ACC_SYNC`.
- Total: 120 execuções.

### Achados de corretude

- 120/120 execuções válidas.
- Nenhuma ocorrência de `NaN` ou `Inf`.
- Nenhuma violação de `Pmin`, `Pmax`, `UR` ou `DR`.
- Maior erro de balanço: aproximadamente `7.3191e-07 MW`.
- `SEQ` e `OMP` foram equivalentes em 40/40 comparações.

### Achados de desempenho

| Sistema | Variáveis | SEQ | OMP@12 | ACC_SYNC | Speedup ACC vs SEQ | ACC vs OMP |
|---:|---:|---:|---:|---:|---:|---:|
| 10 | 240 | 21.71 s | 4.58 s | 3.83 s | 5.67x | 1.20x |
| 100 | 2400 | 215.60 s | 40.27 s | 29.71 s | 7.26x | 1.36x |
| 200 | 4800 | 441.69 s | 84.30 s | 62.87 s | 7.03x | 1.34x |
| 500 | 12000 | 1138.14 s | 222.66 s | 172.54 s | 6.60x | 1.29x |

Com população fixa em 320, a versão `ACC_SYNC` superou o OpenMP com 12 threads em todos os tamanhos de sistema avaliados.

### Tempo em GPU

| Sistema | Kernel GPU / tempo ACC_SYNC | PCIe / tempo ACC_SYNC |
|---:|---:|---:|
| 10 | 88.58% | 4.75% |
| 100 | 93.04% | 4.43% |
| 200 | 93.97% | 3.56% |
| 500 | 94.93% | 2.93% |

A fração de tempo em kernels GPU aumentou com o número de variáveis, enquanto o peso relativo das transferências PCIe diminuiu. Isso indica melhor amortização dos custos de execução em GPU conforme o sistema cresce.

### Achados de qualidade

Gap médio do `ACC_SYNC` em relação a `SEQ`/`OMP`:

| Sistema | Gap médio ACC_SYNC vs SEQ |
|---:|---:|
| 10 | -0.61% |
| 100 | -0.73% |
| 200 | -0.53% |
| 500 | -0.18% |

O `ACC_SYNC` manteve custo médio menor que `SEQ`/`OMP`, mas o ganho percentual diminuiu no sistema de 500 unidades.

### Conclusão

A Fase C confirmou que, uma vez fixada a população em uma região favorável à GPU, a versão `ACC_SYNC` supera o OpenMP em todos os tamanhos de sistema testados. O crescimento do tempo foi aproximadamente linear com o número de variáveis, e a GPU manteve vantagem de desempenho mesmo no sistema de 500 unidades.

O speedup da GPU atingiu o maior valor relativo no sistema de 100 unidades e reduziu levemente em 200 e 500 unidades. Essa redução pode estar associada à maior pressão sobre recursos de memória/cache ou saturação de recursos da GPU, mas os dados não indicam aumento relativo de custo de PCIe.

## Conclusão integrada das Fases A, B e C

Os resultados das três fases sustentam as seguintes conclusões:

1. A implementação OpenMP é uma aceleração fiel da versão sequencial, preservando equivalência numérica em todos os testes realizados.
2. A versão OpenACC síncrona é funcionalmente correta, respeitando as restrições do problema e evitando `NaN`/`Inf`.
3. A GPU precisa de carga computacional suficiente para compensar seus overheads. Esse ponto foi observado na Fase B entre 200 e 240 vagalumes, consistente nas 5 sementes testadas em ambas as populações. O próprio OpenMP tem teto de escalabilidade baixo (eficiência ≤ ~44% em 12 threads), o que também contribui para a GPU vencer a partir desse ponto.
4. Com população fixada acima do ponto de cruzamento, a GPU supera o OpenMP em todos os tamanhos de sistema da Fase C.
5. A qualidade da solução do `ACC_SYNC` foi sistematicamente competitiva, frequentemente com custo médio menor que `SEQ`/`OMP`, embora o gap diminua em sistemas maiores.

Assim, as Fases A, B e C fornecem uma base consistente para as próximas etapas do estudo: análise estatística de qualidade da solução e medições finais de desempenho com repetição controlada.
