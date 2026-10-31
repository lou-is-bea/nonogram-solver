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

`baselines/<machine>.json` est une sortie JSON du harnais, avec les options par défaut, sur tout le corpus. Elle sert de référence à `bench_compare.py` et à la porte G10 (ticket #14).

| Fichier | Machine | Mesurée le |
|---|---|---|
| `baselines/ryzen-9800x3d-wsl.json` | AMD Ryzen 7 9800X3D (8 cœurs), Ubuntu 24.04 sous WSL2, GCC 14, preset `release` | 2026-10-31 |

Une référence n'est mise à jour qu'en fusionnant une PR qui justifie le changement. Pour la régénérer :

```sh
build/release/src/cli/nonogram bench bench/corpus/tiny/*.non --format json > bench/baselines/ryzen-9800x3d-wsl.json
```

Deux mesures successives sur cette machine donnent les mêmes compteurs, avec des médianes qui varient jusqu'à environ 10 % sur les puzzles les plus courts (moins de 200 µs) et de quelques pour cent au-delà. Le seuil de régression de G10 devra en tenir compte.
