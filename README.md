# nonogram-solver

Moteur de résolution de nonogrammes (picross, hanjie) écrit en **C23** pour Linux. L'empreinte mémoire est maîtrisée et l'architecture est modulaire, pour pouvoir comparer plusieurs stratégies de résolution, des plus naïves aux plus optimisées, puis les paralléliser massivement sur des grilles de très grande taille.

> **Statut : version 0.1.0 (M3).** Lecture des puzzles, affichage et vérification des solutions sont disponibles ; les solvers arrivent à partir de M4. L'avancement est suivi dans [docs/ROADMAP.md](docs/ROADMAP.md).

---

## Pourquoi ce projet

Un nonogramme est une grille dont chaque ligne et chaque colonne est annotée par une suite d'indices (les longueurs des blocs de cases noires, dans l'ordre). Résoudre un nonogramme dans le cas général est NP-complet. En pratique, une bonne combinaison de propagation par lignes et de recherche arborescente permet de résoudre très vite la grande majorité des grilles.

Le projet vise trois objectifs :

1. **Un moteur fiable** : chaque solution produite est vérifiée par un vérificateur indépendant.
2. **Un moteur efficace** : la mémoire est allouée de façon explicite et bornée, sans allocation dans les boucles chaudes, et les performances sont mesurées par des benchmarks reproductibles.
3. **Un banc d'essai d'algorithmes** : les solvers se branchent derrière une interface commune, ce qui permet de les comparer sur le même corpus.
4. **Un moteur qui passe à l'échelle** : les types et le modèle de données acceptent des grilles massives (plus de 10¹⁰ cases), et l'architecture prépare une parallélisation sur plusieurs centaines de nœuds de calcul.

## Fonctionnalités prévues

| Étape | Fonctionnalité | Statut |
|---|---|---|
| M1 | Modèle de données (puzzle, grille, indices) et lecture du format `.non` | terminé |
| M2 | Affichage texte du problème et de la solution | terminé |
| M3 | Vérification de la validité d'une solution | terminé (0.1.0) |
| M4 | Solver S0 : force brute (référence pour les petites grilles) | en cours |
| M5 | Solver S1 : backtracking par placements de lignes | à faire |
| M6 | Solver S2 : propagation par lignes jusqu'à point fixe | à faire |
| M7 | Solver S3 : solveur de ligne exact (programmation dynamique) avec propagation et recherche | à faire |
| M8 | Optimisations avancées : sondage, bitsets, comptage de solutions | à faire |
| M9 | Grilles massives, formats binaires, parallélisme en mémoire partagée puis distribué | à faire |

Le détail et les critères de sortie de chaque étape sont dans [docs/ROADMAP.md](docs/ROADMAP.md).

## Démarrage rapide (cible)

> Les commandes de build, de test et `check.sh` sont disponibles, ainsi que `nonogram show`, `nonogram solve`, `nonogram verify`, `nonogram list-solvers` et `nonogram bench`.
>
> Solvers de la bibliothèque : `brute` (S0, force brute sur les cases inconnues, au plus 25 cases ; oracle de référence, [spécification](docs/solvers/s0_brute.md)).

```sh
# Configurer, compiler et tester (profil développement)
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

# Lancer toutes les vérifications avant une contribution
./scripts/check.sh            # dans Ubuntu (WSL2) ou dans le conteneur de référence
```

```sh
# Afficher une grille (disponible), avec une solution, en Unicode
nonogram show tests/data/puzzles/small/duck.non --solution tests/data/solutions/duck.sol --unicode

# Résoudre (solver par défaut : le premier solver complet de list-solvers) et afficher les statistiques
nonogram solve tests/data/puzzles/tiny/letter_p.non --stats

# Choisir le solver, compter jusqu'à 10 solutions, écrire la solution, sortie JSON
nonogram solve tests/data/puzzles/multi/checker_n2.non --solver brute --count 10 --output checker.sol --format json

# Vérifier une solution
nonogram verify tests/data/puzzles/small/duck.non tests/data/solutions/duck.sol

# Lister les solvers disponibles
nonogram list-solvers

# Mesurer un solver sur le corpus de benchmark (temps médian, pic mémoire), sortie JSON
nonogram bench bench/corpus/tiny/*.non --solver brute --format json
```

Les options à valeur s'écrivent `--option valeur` ou `--option=valeur` ; une valeur qui commence par `--` doit utiliser la seconde forme.

Options de `solve` : `--solver <nom>`, `--count <n>` (compter jusqu'à `n` solutions ; « at least » quand le compte est une borne inférieure), `--max-nodes <n>`, `--max-time <ms>`, `--max-memory <octets>`, `--output <grille.sol>` (écrite seulement si le puzzle est résolu ; en cas d'échec d'écriture, code 3 et fichier non fiable), `--stats` et `--format text|json` ([FILE_FORMATS.md §4](docs/FILE_FORMATS.md#4-sorties-de-la-cli)).

Options de `bench` : `--solver <nom>`, `--runs <n>` (exécutions mesurées, 11 par défaut, au plus 1000), `--warmup <n>` (exécutions de chauffe, 2 par défaut), `--seed <n>` et `--format text|json` ; le harnais est décrit dans [bench/README.md](bench/README.md).

Codes de sortie de `nonogram` (exigence F-41) : `0` succès (pour `solve` : résolu), `1` puzzle insoluble, solution invalide ou incomplète (pour `solve` : insoluble, solver bloqué ou limite atteinte), `2` usage incorrect (pour `solve` et `bench`, y compris un puzzle que le solver ne peut pas prendre dans ses limites : taille, `--max-memory`), `3` erreur d'entrée-sortie (fichier illisible ou mal formé, grille de mauvaise taille, sortie impossible à écrire), `4` erreur interne (y compris le manque de mémoire, et pour `bench` un solver qui rompt son contrat). Pour `bench`, le résultat des puzzles ne change pas le code : `0` dès que tout est mesuré.

### Prérequis

- Linux. Le développement se fait sous **Ubuntu dans WSL2** (24.04 LTS, avec GCC 14 et Clang 20).
- Un compilateur C23 : GCC ≥ 14 ou Clang ≥ 19.
- CMake ≥ 3.21 et Ninja.
- Criterion ≥ 2.4 (`libcriterion-dev`) pour les tests.
- Pour la pipeline complète : clang-format, clang-tidy, cppcheck, Valgrind et gcovr.

La liste exacte des paquets est dans [CONTRIBUTING.md](CONTRIBUTING.md#2-mise-en-place-de-lenvironnement). L'image `tools/docker/Dockerfile` (à partir de M0) fournit le même environnement que la CI.

## Organisation du dépôt (cible)

```
include/nonogram/   API publique (en-têtes installables)
src/core/           modèle de données, statuts, allocateur
src/io/             lecture et écriture des formats de fichiers
src/render/         affichage texte
src/verify/         vérificateur indépendant
src/solvers/        un sous-dossier par solver et le registre
src/cli/            l'exécutable `nonogram`
tests/              tests unitaires, d'intégration, de propriétés, fuzzing, données
bench/              corpus de benchmark et références de performance
docs/               documentation (humains et agents)
```

Le détail des modules et de leurs dépendances est dans [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## Documentation

| Document | Public | Contenu |
|---|---|---|
| [docs/README.md](docs/README.md) | tous | Index et parcours de lecture |
| [docs/REQUIREMENTS.md](docs/REQUIREMENTS.md) | humains | Exigences fonctionnelles et non fonctionnelles |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | tous | Modules, structures de données, interfaces |
| [docs/ROADMAP.md](docs/ROADMAP.md) | tous | Étapes, livrables, critères de sortie |
| [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) | tous | Règles de code C, mémoire, erreurs, style |
| [docs/TESTING.md](docs/TESTING.md) | tous | Pipeline de validation et portes de qualité |
| [docs/FILE_FORMATS.md](docs/FILE_FORMATS.md) | tous | Formats `.non` et `.sol` |
| [docs/GLOSSARY.md](docs/GLOSSARY.md) | tous | Vocabulaire du domaine |
| [docs/adr/](docs/adr/README.md) | tous | Décisions d'architecture (ADR) |
| [CONTRIBUTING.md](CONTRIBUTING.md) | humains | Flux de travail, revue, définition de « terminé » |
| [AGENTS.md](AGENTS.md) | agents IA | Règles et procédures pour les agents |

## Contribuer

Toute modification passe par une branche, une pull request, la pipeline de validation complète et une revue humaine. Voir [CONTRIBUTING.md](CONTRIBUTING.md).

## Licence

À définir (voir la question ouverte Q1 dans [docs/ROADMAP.md](docs/ROADMAP.md)).
