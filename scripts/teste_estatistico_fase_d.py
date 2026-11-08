#!/usr/bin/env python3
"""
Teste estatistico da Fase D: compara o custo final de SEQ/OMP contra ACC_SYNC.

Uso:
    python3 scripts/teste_estatistico_fase_d.py results/phase_d/20260811_112604

Le summary.csv da pasta indicada (apenas linhas com valid_ok == 1) e grava:
    <pasta>/teste_estatistico.csv  (uma linha por teste)
    <pasta>/teste_estatistico.txt  (versao legivel, com metadados)

Nao modifica summary.csv, stats.csv nem qualquer outro arquivo existente.
"""

import csv
import datetime
import subprocess
import sys
from pathlib import Path

try:
    import scipy
    from scipy import stats as sstats
except ImportError:
    sys.stderr.write(
        "ERRO: o pacote 'scipy' nao esta instalado neste Python.\n"
        "Este script depende dele para os testes de Mann-Whitney e Wilcoxon.\n\n"
        "Para instalar (apos sua confirmacao), uma opcao e:\n"
        "  sudo apt install python3-pip python3-scipy\n"
        "ou, em um ambiente virtual:\n"
        "  python3 -m venv .venv && source .venv/bin/activate && pip install scipy\n"
    )
    sys.exit(1)


def read_summary(run_dir: Path):
    """Le summary.csv e retorna {version: {seed: cost}} apenas para valid_ok == 1."""
    summary_path = run_dir / "summary.csv"
    data = {}
    with open(summary_path, newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            if row["valid_ok"] != "1":
                continue
            version = row["version"]
            seed = row["seed"]
            cost = float(row["cost"])
            data.setdefault(version, {})[seed] = cost
    return data


def check_seq_omp_identical(data):
    """Verifica se SEQ e OMP tem custo identico por semente. Nao interrompe o script."""
    seq = data.get("SEQ", {})
    omp = data.get("OMP", {})
    common_seeds = sorted(set(seq) & set(omp), key=int)
    mismatches = []
    for s in common_seeds:
        if seq[s] != omp[s]:
            mismatches.append((s, seq[s], omp[s]))
    if mismatches:
        print(
            f"AVISO: SEQ e OMP divergem em {len(mismatches)}/{len(common_seeds)} sementes "
            "(custos NAO identicos). Prosseguindo com SEQ como referencia.",
            file=sys.stderr,
        )
        for s, cs, co in mismatches[:10]:
            print(f"  seed={s}: SEQ={cs!r} OMP={co!r}", file=sys.stderr)
    else:
        print(
            f"OK: SEQ e OMP tem custo identico nas {len(common_seeds)} sementes em comum."
        )
    return len(mismatches) == 0


def has_ties(a, b):
    combined = sorted(list(a) + list(b))
    return len(set(combined)) != len(combined)


def mannwhitney_test(seq_vals, acc_vals, alternative):
    """
    x = SEQ, y = ACC_SYNC.
    alternative='greater' testa: custo(SEQ) estocasticamente MAIOR que custo(ACC_SYNC),
    ou seja, equivalente a "custo de ACC_SYNC e menor".
    Usa method='exact' quando nao ha empates entre as duas amostras; caso contrario
    usa method='asymptotic' e registra isso na saida.
    """
    ties = has_ties(seq_vals, acc_vals)
    method = "asymptotic" if ties else "exact"
    try:
        res = sstats.mannwhitneyu(
            seq_vals, acc_vals, alternative=alternative, method=method
        )
    except ValueError:
        method = "asymptotic"
        res = sstats.mannwhitneyu(
            seq_vals, acc_vals, alternative=alternative, method=method
        )
    return res.statistic, res.pvalue, method, ties


def wilcoxon_test(seq_paired, acc_paired, alternative):
    """
    Wilcoxon de postos sinalizados, pareado por semente. SEQ - ACC_SYNC.
    Apenas analise de sensibilidade (teste principal e o Mann-Whitney).
    """
    diffs = [s - a for s, a in zip(seq_paired, acc_paired)]
    nonzero = [d for d in diffs if d != 0]
    ties_nonzero = len(set(abs(d) for d in nonzero)) != len(nonzero)
    method = "asymptotic" if (len(diffs) != len(nonzero) or ties_nonzero) else "exact"
    try:
        res = sstats.wilcoxon(
            seq_paired, acc_paired, alternative=alternative, method=method
        )
    except ValueError:
        method = "asymptotic"
        res = sstats.wilcoxon(
            seq_paired, acc_paired, alternative=alternative, method=method
        )
    return res.statistic, res.pvalue, method


def pairwise_effect_sizes(seq_vals, acc_vals):
    """
    Probabilidade de superioridade de ACC_SYNC (fracao de pares em que ACC_SYNC
    tem custo menor que SEQ) e correlacao rank-biserial (= Delta de Cliff),
    calculadas por contagem direta de pares (sem depender de convencoes internas
    do scipy). r > 0 significa que ACC_SYNC tende a ter custo menor que SEQ.
    """
    n1, n2 = len(seq_vals), len(acc_vals)
    total_pairs = n1 * n2
    acc_better = 0
    seq_better = 0
    for s in seq_vals:
        for a in acc_vals:
            if a < s:
                acc_better += 1
            elif s < a:
                seq_better += 1
    ties = total_pairs - acc_better - seq_better
    prob_superioridade_acc = (acc_better + 0.5 * ties) / total_pairs
    rank_biserial = (acc_better - seq_better) / total_pairs
    return prob_superioridade_acc, rank_biserial, acc_better, seq_better, ties


def mean(xs):
    return sum(xs) / len(xs)


def median(xs):
    ys = sorted(xs)
    n = len(ys)
    mid = n // 2
    if n % 2 == 0:
        return (ys[mid - 1] + ys[mid]) / 2
    return ys[mid]


def git_short_hash(run_dir: Path) -> str:
    try:
        out = subprocess.run(
            ["git", "rev-parse", "--short", "HEAD"],
            cwd=run_dir,
            capture_output=True,
            text=True,
            check=True,
        )
        return out.stdout.strip()
    except Exception:
        return "N/A (git rev-parse falhou)"


def main():
    if len(sys.argv) != 2:
        print(f"Uso: python3 {sys.argv[0]} <pasta_da_rodada>", file=sys.stderr)
        sys.exit(1)

    run_dir = Path(sys.argv[1]).resolve()
    if not run_dir.is_dir():
        print(f"ERRO: pasta nao encontrada: {run_dir}", file=sys.stderr)
        sys.exit(1)

    data = read_summary(run_dir)
    for v in ("SEQ", "OMP", "ACC_SYNC"):
        if v not in data:
            print(f"ERRO: versao '{v}' nao encontrada em summary.csv (valid_ok==1).", file=sys.stderr)
            sys.exit(1)

    seq_omp_ok = check_seq_omp_identical(data)

    seq = data["SEQ"]
    acc = data["ACC_SYNC"]
    common_seeds = sorted(set(seq) & set(acc), key=int)
    dropped_seq = sorted(set(seq) - set(acc), key=int)
    dropped_acc = sorted(set(acc) - set(seq), key=int)

    seq_vals = [seq[s] for s in common_seeds]
    acc_vals = [acc[s] for s in common_seeds]
    n1, n2 = len(seq_vals), len(acc_vals)

    # --- Teste principal: Mann-Whitney U (amostras independentes) ---
    mw_two_stat, mw_two_p, mw_two_method, mw_ties = mannwhitney_test(
        seq_vals, acc_vals, alternative="two-sided"
    )
    mw_one_stat, mw_one_p, mw_one_method, _ = mannwhitney_test(
        seq_vals, acc_vals, alternative="greater"
    )

    # --- Teste complementar: Wilcoxon pareado por semente (sensibilidade) ---
    wi_two_stat, wi_two_p, wi_two_method = wilcoxon_test(
        seq_vals, acc_vals, alternative="two-sided"
    )
    wi_one_stat, wi_one_p, wi_one_method = wilcoxon_test(
        seq_vals, acc_vals, alternative="greater"
    )

    # --- Medidas de efeito ---
    mean_seq, mean_acc = mean(seq_vals), mean(acc_vals)
    med_seq, med_acc = median(seq_vals), median(acc_vals)
    diff_mean = mean_acc - mean_seq
    diff_mean_pct = diff_mean / mean_seq * 100
    diff_median = med_acc - med_seq
    diff_median_pct = diff_median / med_seq * 100

    prob_sup_acc, rank_biserial, acc_better, seq_better, ties_n = pairwise_effect_sizes(
        seq_vals, acc_vals
    )

    best_seq = min(seq_vals)
    worst_acc = max(acc_vals)
    nao_sobrepoe = worst_acc < best_seq

    commit_hash = git_short_hash(run_dir)
    scipy_version = scipy.__version__
    now = datetime.datetime.now().isoformat(timespec="seconds")

    # --- Grava teste_estatistico.csv ---
    csv_path = run_dir / "teste_estatistico.csv"
    with open(csv_path, "w", newline="") as f:
        writer = csv.writer(f)
        writer.writerow(["nome", "n1", "n2", "estatistica", "p_valor", "metodo", "hipotese_alternativa"])
        writer.writerow([
            "mannwhitney_seq_vs_accsync", n1, n2, mw_two_stat, mw_two_p, mw_two_method,
            "two-sided (distribuicoes diferentes)",
        ])
        writer.writerow([
            "mannwhitney_seq_vs_accsync", n1, n2, mw_one_stat, mw_one_p, mw_one_method,
            "greater (custo SEQ > custo ACC_SYNC, i.e. ACC_SYNC menor)",
        ])
        writer.writerow([
            "wilcoxon_pareado_seq_vs_accsync_sensibilidade", n1, n2, wi_two_stat, wi_two_p, wi_two_method,
            "two-sided (distribuicoes diferentes)",
        ])
        writer.writerow([
            "wilcoxon_pareado_seq_vs_accsync_sensibilidade", n1, n2, wi_one_stat, wi_one_p, wi_one_method,
            "greater (custo SEQ > custo ACC_SYNC, i.e. ACC_SYNC menor)",
        ])

    # --- Grava teste_estatistico.txt ---
    txt_path = run_dir / "teste_estatistico.txt"
    lines = []
    lines.append("Teste estatistico - Fase D (SEQ/OMP vs ACC_SYNC)")
    lines.append(f"Pasta da rodada: {run_dir}")
    lines.append(f"Data de execucao deste script: {now}")
    lines.append(f"Commit git (curto): {commit_hash}")
    lines.append(f"Versao do scipy: {scipy_version}")
    lines.append("")
    lines.append("Verificacao SEQ vs OMP (custo identico por semente):")
    lines.append(f"  {'OK: custos identicos' if seq_omp_ok else 'AVISO: custos NAO identicos, ver stderr. Usando SEQ como referencia.'}")
    lines.append("")
    lines.append(f"Amostras usadas (apenas valid_ok==1, sementes em comum entre SEQ e ACC_SYNC): n1=n2={n1}")
    if dropped_seq:
        lines.append(f"  Sementes de SEQ sem par em ACC_SYNC (descartadas): {dropped_seq}")
    if dropped_acc:
        lines.append(f"  Sementes de ACC_SYNC sem par em SEQ (descartadas): {dropped_acc}")
    lines.append("")
    lines.append("TESTE PRINCIPAL: Mann-Whitney U (amostras independentes, SEQ vs ACC_SYNC)")
    lines.append("  Motivo: as trajetorias de SEQ e ACC_SYNC sao diferentes (ACC_SYNC usa movimento")
    lines.append("  sincronizado), entao o pareamento por semente nao reflete amostras dependentes")
    lines.append("  no sentido classico do teste pareado. Por isso o teste independente e o principal.")
    lines.append(f"  Bilateral: U={mw_two_stat}, p={mw_two_p:.6g}, method={mw_two_method}")
    lines.append(f"  Unilateral (H1: custo ACC_SYNC menor que SEQ): U={mw_one_stat}, p={mw_one_p:.6g}, method={mw_one_method}")
    if mw_ties:
        lines.append("  AVISO: havia empates entre as amostras SEQ e ACC_SYNC; method='exact' nao e valido, usado 'asymptotic'.")
    lines.append("")
    lines.append("TESTE COMPLEMENTAR (sensibilidade): Wilcoxon de postos sinalizados, pareado por semente")
    lines.append("  Apenas analise de sensibilidade; o teste principal e o Mann-Whitney acima.")
    lines.append(f"  Bilateral: W={wi_two_stat}, p={wi_two_p:.6g}, method={wi_two_method}")
    lines.append(f"  Unilateral (H1: custo ACC_SYNC menor que SEQ): W={wi_one_stat}, p={wi_one_p:.6g}, method={wi_one_method}")
    lines.append("")
    lines.append("MEDIDAS DE EFEITO")
    lines.append(f"  Media SEQ: {mean_seq:.6f}")
    lines.append(f"  Media ACC_SYNC: {mean_acc:.6f}")
    lines.append(f"  Diferenca de medias (ACC_SYNC - SEQ): {diff_mean:.6f} ({diff_mean_pct:.4f}%)")
    lines.append(f"  Mediana SEQ: {med_seq:.6f}")
    lines.append(f"  Mediana ACC_SYNC: {med_acc:.6f}")
    lines.append(f"  Diferenca de medianas (ACC_SYNC - SEQ): {diff_median:.6f} ({diff_median_pct:.4f}%)")
    lines.append(
        f"  Probabilidade de superioridade de ACC_SYNC (fracao de pares SEQ x ACC_SYNC em que"
        f" ACC_SYNC tem custo menor): {prob_sup_acc:.6f}"
        f"  (pares ACC_SYNC menor={acc_better}, pares SEQ menor={seq_better}, empates={ties_n}, total={n1*n2})"
    )
    lines.append(
        f"  Correlacao rank-biserial (= Delta de Cliff; positivo => ACC_SYNC tende a custo menor): {rank_biserial:.6f}"
    )
    lines.append("")
    lines.append("SOBREPOSICAO DAS DISTRIBUICOES")
    lines.append(f"  Melhor custo (minimo) de SEQ: {best_seq:.6f}")
    lines.append(f"  Pior custo (maximo) de ACC_SYNC: {worst_acc:.6f}")
    if nao_sobrepoe:
        lines.append("  As distribuicoes NAO se sobrepoem: todo ACC_SYNC < todo SEQ nesta amostra.")
    else:
        lines.append("  As distribuicoes se sobrepoem (ha pelo menos um ACC_SYNC >= algum SEQ).")
    lines.append("  (Conferir estes dois valores com best_cost/worst_cost em stats.csv.)")
    lines.append("")

    with open(txt_path, "w") as f:
        f.write("\n".join(lines) + "\n")

    print(f"\nArquivos gravados:\n  {csv_path}\n  {txt_path}")


if __name__ == "__main__":
    main()
