# Resultados Experimentais — rascunho para o TCC

> Rascunho baseado nos dados já coletados em `bench_f*.txt` /
> `docs/resultados_consolidados.csv`. Ajustar números de hardware
> (seção "Ambiente") antes de usar no texto final, e revisar o texto
> em prosa — está propositalmente factual e sem floreios, para vocês
> adaptarem ao estilo de escrita do TCC.

## 1. Ambiente de execução

Os testes foram executados em [PREENCHER: CPU, nº de núcleos/threads, GPU,
driver] com compilador `gcc` (CPU) e `nvc` (GPU, OpenACC). Todas as versões
foram compiladas com otimização `-O3`. A versão OpenMP utilizou
`OMP_NUM_THREADS` igual ao número de threads lógicas disponíveis (12, no
ambiente de coleta).

O dataset utilizado nos experimentos de desempenho é o sistema de **100
unidades geradoras** (`data/units_victoire100.csv`), com perfil de carga de
24 horas (`data/load_24h_victoire100.csv`).

## 2. Corretude do modelo

Antes de qualquer avaliação de desempenho, o modelo foi validado com
`bin_validate`, confirmando violação nula de balanço de potência
(`Max |sumP - load| = 0`) e de limites de rampa (`Max violação de rampa
(UR/DR) = 0`) para o sistema de referência. Isso garante que as comparações
de tempo/speedup entre versões dizem respeito a soluções igualmente válidas.

## 3. Escalabilidade com o tamanho do enxame (nº de vagalumes)

Com `seed=42` e `1000` iterações fixas, variou-se o número de vagalumes de
160 a 480. A Tabela 1 resume o speedup obtido pelas versões OpenMP (CPU, 12
threads) e OpenACC (GPU) em relação à versão sequencial.

**Tabela 1 — Speedup vs. número de vagalumes (seed=42, 1000 iterações)**

| Vagalumes | Speedup OpenMP | Speedup OpenACC (GPU) | Gap de custo GPU vs. SEQ |
|-----------|---------------:|-----------------------:|--------------------------:|
| 160       | 5,15x          | 4,39x                   | -0,66%                    |
| 200       | 5,33x          | 5,10x                   | -0,57%                    |
| 240       | 5,47x          | 5,94x                   | -0,63%                    |
| 280       | 5,32x          | 6,44x                   | -0,68%                    |
| 320       | 5,37x          | 7,44x                   | -0,64%                    |
| 400       | 5,49x          | 7,88x                   | -0,76%                    |
| 480       | 5,47x          | 7,91x                   | -0,66%                    |

O "gap de custo" é a diferença percentual entre o melhor custo encontrado
pela versão comparada e o da versão sequencial; valores negativos indicam
que a versão paralela encontrou uma solução de **custo menor** que a
sequencial na mesma rodada (não pior).

**Observações para discussão no texto:**

- O speedup do OpenMP se mantém praticamente constante (~5,2x–5,5x) em toda
  a faixa testada, o que é esperado: com 12 threads e uma carga de trabalho
  já suficientemente grande a partir de ~160 vagalumes, o paralelismo em CPU
  satura rapidamente.
- Já o speedup do OpenACC (GPU) **cresce com o tamanho do enxame** (de 4,39x
  em 160 vagalumes para ~7,9x em 400–480). Isso é o comportamento típico de
  aceleradores GPU: o overhead de transferência de dados (PCIe) é
  proporcionalmente menor quanto maior o volume de cálculo por transferência,
  então o ganho real só aparece com problemas grandes o suficiente.
- A partir de ~320 vagalumes, a GPU passa a superar a CPU (OpenMP) em
  speedup absoluto — um ponto de cruzamento que vale destacar num gráfico.

## 4. Repetibilidade estatística (n=5 seeds, f=320, 1000 iterações)

Para verificar se os resultados acima não são fruto de uma única
inicialização "sortuda", repetiu-se a configuração `f=320, iters=1000` com 5
seeds diferentes (42, 123, 777, 1234, 9999):

| Métrica                  | Média   | Desvio-padrão |
|--------------------------|--------:|---------------:|
| Speedup OpenMP           | 5,36x   | 0,04           |
| Speedup OpenACC (GPU)    | 7,43x   | 0,03           |
| Gap de custo GPU vs. SEQ | -0,71%  | 0,06 p.p.      |

A baixíssima dispersão (desvio-padrão < 0,1 em todas as métricas) indica que
o speedup medido é estável e não depende da seed usada — um resultado forte
para reivindicar consistência do ganho de desempenho.

> **Recomendação antes de fechar o texto**: 5 seeds é um n pequeno para
> "desvio-padrão" soar rigoroso numa banca. Vale rodar mais seeds (10+,
> conforme `docs/protocolo-testes.md`, seção 5.2) antes da versão final do
> TCC, usando o mesmo `bench_compare.sh`.

## 5. Qualidade da solução: paralelização não degrada o resultado

Em todas as rodadas com OpenMP, o custo final foi **idêntico** ao da versão
sequencial (gap de 0,00% em todas as linhas de `bench_f*.txt`), o que é
esperado — a paralelização OpenMP distribui o cálculo de custo/vizinhança
entre threads, mas preserva a mesma ordem de atualização das soluções, logo
o resultado numérico é reprodutível bit a bit em relação ao sequencial.

Já a versão OpenACC (GPU) apresenta um gap **negativo** consistente
(-0,5% a -0,8%), ou seja, encontra soluções de custo *menor*. Isso é
compatível com pequenas diferenças de ordem de operações em ponto flutuante
no acumulador paralelo da GPU, que nesse caso favoreceram levemente a busca
— não indica erro de implementação, mas vale reportar como observação
honesta na discussão, já que idealmente se esperaria paridade de custo,
não melhora sistemática.

## 6. Limitações e próximos passos para fechar o capítulo

1. **Escalabilidade por tamanho de sistema** (10, 200, 500 unidades) ainda
   não foi medida com o `bench_compare.sh` — só o dataset de 100 unidades
   foi testado até agora. Ver seção 5.3 do protocolo de testes.
2. **N de seeds** no experimento de repetibilidade é pequeno (5); considerar
   ampliar para 10+ antes da versão final.
3. Os dados de `units_victoire10.csv`/`units_victoire100.csv` tinham
   coeficientes de custo trocados antes da correção feita no commit
   `d5b6ce7`. Pela data de modificação dos arquivos (`units_victoire100.csv`
   já corrigido é anterior aos `bench_f*.txt` atuais), os resultados acima
   **já usam os dados corrigidos** — não é necessário re-executar por causa
   desse bug. Ainda assim, vale confirmar isso explicitamente ao escrever o
   texto final (ex.: citando o commit da correção como referência de
   reprodutibilidade).
4. Falta o gráfico de speedup vs. tamanho do enxame (Tabela 1) e o de
   dispersão de custo (Seção 4) em formato de figura — gerar a partir de
   `docs/resultados_consolidados.csv` com matplotlib ou similar.
