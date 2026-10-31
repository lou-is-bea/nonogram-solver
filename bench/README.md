# Benchmarks

Ce dossier contient les données des benchmarks : les corpus figés et les fichiers de référence. Le harnais lui-même est la bibliothèque `ng_bench` (`src/bench/`), lancée par la sous-commande `nonogram bench` ([TESTING.md §4.8](../docs/TESTING.md#48-benchmarks)).

## Lancer une mesure

Les mesures de référence se font avec le preset `release` :

```sh
cmake --preset release && cmake --build --preset release
build/release/src/cli/nonogram bench bench/corpus/tiny/*.non --solver brute --format json
```

Pour chaque puzzle, le harnais part d'une grille vide, fait `--warmup` exécutions de chauffe (2 par défaut) puis `--runs` exécutions mesurées (11 par défaut, au plus 1000). Il retient :

- le temps **médian**, le minimum et le maximum des exécutions mesurées, sur l'horloge monotone ;
- le **pic mémoire** d'une exécution, mesuré par un allocateur compteur qui enveloppe l'allocateur de l'appelant : aucune allocation n'échappe au compte ;
- les compteurs du solver (nœuds, résolutions de lignes, solutions, espace de travail).

Chaque grille `SOLVED` passe par le vérificateur (ADR-0007), et toutes les exécutions doivent donner le même résultat et les mêmes compteurs (NF-12). Sinon, la commande s'arrête avec le code 4. La graine (`--seed`, 0 par défaut) est transmise au solver et écrite dans la sortie. Le format des sorties est décrit dans [FILE_FORMATS.md §4](../docs/FILE_FORMATS.md#4-sorties-de-la-cli).

## Horloge

Le harnais mesure le temps avec `clock_gettime(CLOCK_MONOTONIC)`, qu'un changement de l'heure système n'affecte pas. Cette fonction est POSIX : seul `src/bench/clock.c` la voit, avec `_POSIX_C_SOURCE` défini pour ce fichier par CMake, et le reste du projet reste en C23 strict. C23 prévoit `TIME_MONOTONIC` pour `timespec_get`, mais c'est optionnel et la glibc 2.39 de l'environnement de référence ne le fournit pas. Ce choix a été approuvé par le responsable du projet le 2026-10-31 (Linux est la seule plateforme visée).

## Corpus

| Corpus | Contenu | Solvers visés |
|---|---|---|
| `corpus/tiny/` | 10 puzzles d'au plus 25 cases, de 1 à 25 cases inconnues : solution unique (`dot`, `column`, `l_shape`, `frame`, `arrow`), solutions multiples (`diagonal_n2`, `checker_n2`, `steps_n6`, `permutation_n24`), insoluble (`crossed`) | S0 et suivants |

Les fichiers sont des **copies figées** de `tests/data/puzzles/` (tiny, multi, unsolvable) au 2026-10-31 : ils ont la même provenance et la même licence ([tests/data/README.md](../tests/data/README.md)). Un corpus de benchmark ne change pas, pour que les mesures restent comparables ; un nouveau jeu de puzzles fait un nouveau corpus. Le corpus `tiny` s'appelle ainsi, et non `small`, parce qu'il suit la catégorie `tiny` du corpus de test (au plus 25 cases) ; `small` désignera des puzzles jusqu'à 15 × 15, à partir de M5. Le test d'intégration `cli_bench_corpus_tiny` mesure tout le corpus et compare ses compteurs à une sortie de référence.

Les puzzles ont été choisis pour couvrir plusieurs ordres de grandeur de temps avec S0, de quelques dizaines de nanosecondes (`dot`) à quelques dizaines de millisecondes (`arrow`), tout en gardant une mesure complète sous une seconde.

## Références

`baselines/<machine>.json` est une sortie JSON du harnais, avec les options par défaut, sur tout le corpus. Elle sert de référence à la porte G10 en local ([TESTING.md §3](../docs/TESTING.md#3-portes-de-qualité)) :

```sh
NG_BENCH_MACHINE=ryzen-9800x3d-wsl ./scripts/check.sh --only G10
```

La porte mesure le corpus avec le preset `release` et compare le résultat à `baselines/$NG_BENCH_MACHINE.json` avec `scripts/bench_compare.py`. Sans `NG_BENCH_MACHINE`, ou sans fichier pour cette machine, elle est signalée `SKIP`. Une régression bloquante n'échoue la porte que si elle se retrouve dans chacune des 3 mesures (`BENCH_ATTEMPTS`). Le script se lance aussi seul : `scripts/bench_compare.py <référence.json> <mesure.json>` (options `--time-threshold`, `--memory-threshold`, `--time-floor-ns` et `--time-mode block|report`). Il sort avec le code 1 pour une régression bloquante et 2 pour une entrée illisible ou incomparable. En CI, la tâche `bench` compare la PR à sa base sur la même machine, sans fichier de référence.

| Fichier | Machine | Mesurée le |
|---|---|---|
| `baselines/ryzen-9800x3d-wsl.json` | AMD Ryzen 7 9800X3D (8 cœurs), Ubuntu 24.04 sous WSL2, GCC 14, preset `release` | 2026-10-31 |

Une référence n'est mise à jour qu'en fusionnant une PR qui justifie le changement. Pour en ajouter une, ou la régénérer :

```sh
build/release/src/cli/nonogram bench bench/corpus/tiny/*.non --format json > bench/baselines/ryzen-9800x3d-wsl.json
```

Deux mesures successives sur cette machine donnent les mêmes compteurs, avec des médianes qui varient jusqu'à environ 11 % sur les puzzles les plus courts (moins de 200 µs) et de quelques pour cent au-delà. C'est pourquoi une hausse de temps ne bloque que si la médiane de référence atteint 1 ms. Lors d'une autre paire de mesures du même code sur cette machine (2026-10-31), l'écart a même dépassé 30 % sur deux de ces puzzles.
