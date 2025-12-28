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
├── data/           # Arquivos de entrada (dados do sistema)
├── include/        # Arquivos de cabeçalho (.h)
├── src/            # Código-fonte (.c)
│   ├── ded_model.c
│   ├── main_seq.c
│   ├── main_omp.c
│   ├── main_validate.c
│   └── utils.c
│
├── tests/
├── docs/
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

## 🚧 Trabalhos Futuros

- Implementação em GPU
- Inclusão de efeito de válvula
- Análise estatística
- Comparação com outras metaheurísticas

---

## 👤 Autor

**Jorge Lucas** 
Projeto acadêmico – 2025

---

## 📄 Licença

Uso acadêmico e educacional.
