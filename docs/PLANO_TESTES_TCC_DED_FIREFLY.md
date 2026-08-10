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

- populações: 60, 120, 160, 200, 240, 280, 320, 400 e 480;
- iterações: 1.000;
- sementes: no mínimo 10 sementes fixas;
- OpenMP: 1, 2, 4, 8 e 12 threads;
- OpenACC: `bin_acc_sync_bench` (versão oficial do estudo; não usar `bin_acc` aqui).

Objetivo: identificar o ponto em que CPU paralela e GPU amortizam seus custos.

### Fase C — Escalabilidade pelo tamanho do sistema

- sistemas: 10, 100, 200 e 500 unidades;
- população principal: 320 (ou valor definido após a Fase B);
- iterações: 1.000;
- sementes: no mínimo 10 sementes fixas;
- versões: SEQ, OpenMP e OpenACC (`bin_acc_sync_bench`).

Objetivo: avaliar escalabilidade em função do número de variáveis do DED.

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
