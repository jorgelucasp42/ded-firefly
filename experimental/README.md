# Variantes de GPU descartadas

Estes arquivos não fazem parte do estudo (não entram em nenhuma tabela/figura do TCC) e não têm
mais alvo no `Makefile`. Ficam aqui só como histórico das abordagens de OpenACC testadas antes de
se chegar em `firefly_openacc_sync.c`, que é a única versão OpenACC usada no trabalho (ver
`docs/PLANO_TESTES_TCC_DED_FIREFLY.md`, seção 1).

- `firefly_openacc.c` + `main_acc.c` / `main_acc_100.c` / `main_acc_200.c` / `main_acc_500.c` —
  versão "de referência": roda o laço de movimento/reparo em CPU (serial, igual ao SEQ/OpenMP) e só
  manda o cálculo de custo pra GPU. Tem uma divergência real de trajetória em relação ao SEQ (o
  `solution_repair` é chamado dentro de `move_firefly`, uma vez por vizinho atrator, em vez de uma
  vez por vagalume) — por isso nunca foi tratada como equivalente de fato, mesmo antes de ser
  descartada.
- `firefly_openacc_full.c` — outra tentativa de mover mais trabalho para kernels GPU, anterior à
  versão `sync`.
- `firefly_openacc_pop.c` + `main_acc_pop.c` — variante que paraleliza por população; não chegou a
  ser adotada.
- `main_acc_sync_100.c` — variante fixa (100 unidades, sem `n_fireflies`/`iters` por CLI) da versão
  `sync`; redundante com `main_acc_sync_bench.c`, compilado pelo alvo `bench_acc_sync` como `bin_acc_sync_bench`.

Para recompilar qualquer um deles (fora do fluxo normal do projeto), é preciso apontar o `Makefile`
manualmente para estes caminhos.
