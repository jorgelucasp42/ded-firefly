# ded-firefly

Implementação do **Despacho Econômico Dinâmico (DED)** utilizando a metaheurística **Firefly Algorithm**, com versões **sequencial** e **paralela (OpenMP)**, voltada para estudos de desempenho e qualidade de solução em sistemas de potência.

Este projeto faz parte de um **trabalho acadêmico/TCC**, com foco em otimização, computação paralela e aplicações em sistemas elétricos de potência.

---

## 📌 Objetivo

O objetivo do projeto é resolver o **Problema de Despacho Econômico Dinâmico (DED)** minimizando o custo de geração, considerando:

- Restrições de potência mínima e máxima dos geradores
- Continuidade temporal da geração
- Avaliação do impacto da **paralelização com OpenMP**
- Comparação entre abordagem **sequencial** e **paralela**

---

## ⚙️ Tecnologias Utilizadas

- Linguagem **C**
- **OpenMP** para paralelização
- Compilação via **Makefile**
- Ambiente Linux (Ubuntu)

---

## 📁 Estrutura do Projeto

```text
ded-firefly/
│
├── data/               # Arquivos de entrada (unidades e curva de carga), tamanhos: 10/100/200/500
├── include/            # Cabeçalhos (.h)
├── src/
│   ├── core/           # ded_data.c, ded_model.c, utils.c (modelo DED e utilitários, comum a todas as versões)
│   ├── firefly/        # firefly_seq.c, firefly_omp.c, firefly_openacc*.c (motor da metaheurística por versão)
│   └── apps/           # main_seq*.c, main_omp*.c, main_acc*.c, main_validate.c (pontos de entrada/CLI)
│
├── bin/                # Binários compilados (gerado pelo make, não versionado)
├── bench_compare.sh    # Compara SEQ vs OMP vs ACC para uma dada configuração
├── grid_search.sh      # Varredura de hiperparâmetros (alpha/gamma) do Firefly
├── bench_f*.txt        # Resultados brutos de benchmarks já executados
├── docs/               # PLANO_TESTES_TCC_DED_FIREFLY.md, protocolo de testes e resultados consolidados para o TCC
├── Makefile
├── .gitignore
└── README.md
```

---

## 🔧 Compilação

No Ubuntu:

```bash
sudo apt update
sudo apt install build-essential
```

Compilar:

```bash
make
```

---

## ▶️ Execução

Sequencial:
```bash
./main_seq
```

Paralela:
```bash
export OMP_NUM_THREADS=4
./main_omp
```

---

## 📊 Resultados Esperados

- Comparação de tempo de execução
- Avaliação da convergência do Firefly Algorithm
- Análise de custo total
- Estudo do impacto do paralelismo

---

## 📚 Contexto Acadêmico

Metaheurísticas aplicadas a sistemas elétricos, despacho econômico dinâmico e computação paralela.

---

## 👤 Autor

**Jorge Lucas** 
Projeto acadêmico – 2025

---

## 📄 Licença

Uso acadêmico.
