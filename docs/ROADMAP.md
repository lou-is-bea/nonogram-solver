# Feuille de route

> Dernière revue : 2026-10-25.

Le projet avance par **étapes** (M0 à M9). Chaque étape a un périmètre fermé, des livrables et des **critères de sortie**. On ne démarre pas une étape tant que la précédente ne les a pas tous remplis. Une étape peut se découper en plusieurs PR.

| Étape | Thème | Version visée | Statut |
|---|---|---|---|
| M0 | Fondations : build, CI, outillage | — | terminé (validation locale) |
| M1 | Modèle de données (types pour grilles massives) et lecture `.non` en flux | — | à faire |
| M2 | Affichage | — | à faire |
| M3 | Vérificateur | 0.1.0 | à faire |
| M4 | S0 : force brute, et harnais de benchmark | 0.2.0 | à faire |
| M5 | S1 : backtracking par placements de lignes | 0.3.0 | à faire |
| M6 | S2 : propagation par lignes | 0.4.0 | à faire |
| M7 | S3 : ligne exacte (programmation dynamique), propagation et recherche | 0.5.0 | à faire |
| M8 | Optimisations avancées (séquentielles) | 0.6.0 | à faire |
| M9 | Grilles massives et parallélisme | 0.7.0 et suivantes | à faire |

---

## M0 — Fondations

**Objectif** : disposer d'un squelette qui compile, se teste et se vérifie automatiquement, avant d'écrire la moindre logique métier.

**Livrables**

- `tools/docker/Dockerfile` : l'environnement de référence (Ubuntu 24.04 LTS, GCC 14, Clang 20 et 19, CMake, Ninja, Criterion, Valgrind, clang-format, clang-tidy, cppcheck, gcovr, Python 3), aux versions figées.
- `CMakeLists.txt` racine et par module, avec les cibles `ng_core`, `ng_io`, `ng_render`, `ng_verify`, `ng_solvers` et `nonogram`. Au départ, les cibles sont vides ou contiennent un simple fichier `version.c`. La configuration échoue si le compilateur ne supporte pas `-std=c23`.
- `CMakePresets.json` avec les presets `dev`, `dev-clang`, `release`, `asan`, `ubsan`, `coverage` et `fuzz`, avec le générateur Ninja.
- Les fichiers de configuration `.clang-format`, `.clang-tidy` et `.editorconfig`, conformes à [CODING_STANDARDS.md](CODING_STANDARDS.md).
- Criterion détecté via `pkg-config` ([ADR-0003](adr/0003-framework-de-test-criterion.md)), la fixture commune de `tests/support/`, un premier test (`test_version.c`) et `tests/valgrind/criterion.supp` (vide ou minimal, approuvé par un humain).
- Les scripts `scripts/check.sh` et `scripts/format.sh`.
- La CI GitHub Actions dans le conteneur de référence ([TESTING.md §5](TESTING.md#5-intégration-continue)).
- La protection de la branche `main` (le gabarit de PR est déjà en place dans `.github/`).

**Critères de sortie**

> **Décision du 2026-10-25** : la CI GitHub Actions n'est pas utilisée pour l'instant. La validation se fait uniquement en local, avec `./scripts/check.sh` dans WSL. Le fichier `.github/workflows/ci.yml` et l'image `tools/docker/Dockerfile` restent dans le dépôt mais n'ont pas encore été exécutés. Les critères qui en dépendent sont différés jusqu'à l'activation de la CI.

- ✅ Le build et les tests passent avec GCC 14, Clang 20 et Clang 19 (en local).
- ✅ `check.sh` tourne de bout en bout dans WSL. *(différé : exécution dans le conteneur)*
- ✅ Un warning, une erreur de format et une fuite introduits volontairement sont **bloqués** par `check.sh`, à chacune des portes concernées (vérifié en local, ainsi qu'un test en échec, `NULL`, un débordement signé et un accès hors limites). *(différé : la même vérification par la CI sur une PR)*
- *(différé)* La CI est verte sur toute la matrice, et la branche `main` est protégée.

## M1 — Modèle de données

**Objectif** : représenter un puzzle et une grille, de taille quelconque jusqu'aux grilles massives, et lire des puzzles en flux. Exigences F-01 à F-06, F-08 et NF-14.

**Livrables**

- `core` :
  - `types.h` ([ADR-0009](adr/0009-types-et-grilles-massives.md)) et ses `static_assert` ;
  - `ng_status`, `ng_allocator` (allocateur par défaut), `ng_reader` et `ng_writer` ;
  - `ng_puzzle` et `ng_grid`, avec l'accès par case **et** par copie de ligne.
- `tests/support` : allocateurs défaillant, compteur et plafonné, générateur pseudo-aléatoire avec graine (xoshiro256**), fixture Criterion.
- `io` : analyseur `.non` en flux, lecture et écriture `.sol` en flux ([ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md), §1).
- Un premier corpus `tests/data/puzzles/{tiny,small,invalid}` d'au moins 20 fichiers, chacun avec sa solution connue.
- La cible de fuzzing `fuzz_non_parser`.
- Les premiers tests massifs : création d'une grille 10⁴ × 10⁴, aller-retour par lignes, limites vérifiées avec l'allocateur plafonné.

**Critères de sortie**

- Couverture d'au moins 90 % sur `core` et `io`.
- Chaque chemin d'échec d'allocation est testé (boucle sur l'échec de la n-ième allocation).
- Aucun débordement entier sous UBSan avec `-fsanitize=integer`, y compris sur les dimensions supérieures à 2¹⁶.
- Le fuzzer tourne 10 minutes sans aucun plantage.

## M2 — Affichage

**Objectif** : représenter le problème et sa solution de façon lisible. Exigences F-10 à F-13.

**Livrables**

- `render` : `ng_render_text`, en ASCII et en Unicode, avec ou sans indices.
- CLI : la sous-commande `nonogram show <puzzle> [--solution <sol>] [--unicode]`. Au-delà de `--max-render` (200 × 200 par défaut), elle affiche un résumé au lieu de la grille.
- Des tests d'intégration comparés à des sorties de référence (`tests/data/expected/*.txt`).

Exemple de rendu attendu (ASCII) :

```
        1
      3 1 3 1 1
    +-----------+
  3 | # # # . . |
1 1 | # . # . . |
  3 | # # # . . |
  1 | . . . # . |
  1 | . . . . # |
    +-----------+
```

**Critères de sortie** : les sorties de référence sont relues et approuvées par un humain. L'affichage d'une grille 100×100 tient en moins de 2 ms.

## M3 — Vérificateur

**Objectif** : disposer de l'oracle qui valide toute solution. Exigences F-20 à F-22.

**Livrables**

- `verify` : `ng_verify`, avec un rapport détaillé.
- CLI : `nonogram verify <puzzle> <solution>`.
- Tests de propriétés du vérificateur ([TESTING.md §4.4](TESTING.md#44-tests-de-propriétés)).

**Critères de sortie** : couverture d'au moins 95 % sur `verify` (c'est l'oracle, il doit être irréprochable). Les propriétés passent sur 100 000 grilles aléatoires. La vérification d'une grille 10⁴ × 10⁴ est mesurée et consignée. **Publication de la version 0.1.0.**

## M4 — S0 : force brute et harnais de benchmark

**Objectif** : un premier solver volontairement naïf, qui sert d'oracle de référence sur les petites grilles, et l'outil de mesure qui servira à toutes les étapes suivantes.

**Algorithme** : énumérer les `2^(largeur×hauteur)` grilles possibles, dans l'ordre binaire, et vérifier chacune. Le solver refuse les grilles de plus de `NG_BRUTE_MAX_CELLS` cases (25 par défaut) et renvoie alors `NG_ERR_LIMIT`.

**Livrables**

- `src/solvers/s0_brute/`, et sa spécification `docs/solvers/s0_brute.md`.
- L'infrastructure solver : `solver.h`, le registre et `ng_solve`.
- CLI : `solve` et `list-solvers`.
- `bench/` : le harnais (temps médian sur N exécutions, pic mémoire via l'allocateur compteur, sortie JSON), le script `bench_compare.py` et un premier fichier de référence.

**Critères de sortie** : S0 résout tout le corpus `tiny` et le vérificateur valide chaque solution. Le comptage de solutions est correct sur les puzzles à solutions multiples du corpus.

## M5 — S1 : backtracking par placements de lignes

**Algorithme** : pour chaque rangée, on énumère les placements de blocs compatibles avec ses indices. Une recherche en profondeur (pile explicite) choisit un placement par rangée. Après chaque choix, on élague en vérifiant que chaque colonne reste **compatible** avec un préfixe valide de ses indices.

**Critères de sortie** : les résultats sont identiques à ceux de S0 sur tout le corpus `tiny` (test différentiel). Le corpus `small` (jusqu'à 15×15) est résolu, avec des performances mesurées et consignées.

## M6 — S2 : propagation par lignes

**Algorithme** : solveur de ligne par énumération des placements, ou par chevauchement gauche-droite. Il calcule les cases certaines (pleines ou vides) de chaque ligne compte tenu des cases déjà connues. Une **file de lignes modifiées** permet d'itérer jusqu'au point fixe. Le solver est **incomplet** : il renvoie `INCOMPLETE` s'il se bloque.

**Critères de sortie** : il n'y a aucune contradiction avec S0 et S1 (toute case déduite correspond à toutes les solutions connues). L'espace de travail est borné, d'une taille documentée, et aucune allocation n'a lieu pendant la résolution (vérifié avec l'allocateur compteur).

## M7 — S3 : ligne exacte et recherche

**Algorithme**

- **Solveur de ligne exact** par programmation dynamique, en `O(n·k)` par ligne (n la longueur, k le nombre de blocs). Il calcule pour chaque case si elle peut être pleine ou vide dans au moins un placement valide.
- **Propagation** jusqu'au point fixe, comme dans S2.
- Si la propagation se bloque, **recherche** : choix d'une case par heuristique (par exemple la ligne la plus contrainte), puis essai d'une valeur. Les retours arrière s'appuient sur un *trail*, la pile des modifications à défaire, sans copie de la grille.

**Critères de sortie** : les résultats sont identiques à ceux des solvers complets sur `tiny` et `small`. Les objectifs de [REQUIREMENTS.md §4](REQUIREMENTS.md#4-objectifs-de-performance-indicatifs) sont atteints sur le corpus `large`.

## M8 — Optimisations avancées, séquentielles (à préciser)

Chaque piste est évaluée et décrite dans un ADR **avant** d'être implémentée, et son gain est démontré par les benchmarks :

- représentation des lignes en bitsets dans l'espace de travail des solvers (`<stdbit.h>`, mots de 64 bits) ;
- **sondage** (*probing*) : essayer une valeur, propager, et en déduire la valeur opposée en cas de contradiction ;
- mémoïsation des résolutions de lignes, avec un cache de taille fixe ;
- heuristiques de choix avancées ;
- comparaison avec un encodage SAT, à titre de référence externe et hors du moteur ;
- comptage exhaustif de solutions et preuve d'unicité.

## M9 — Grilles massives et parallélisme (à préciser)

**Objectif** : résoudre et mesurer des grilles massives sur une machine, puis sur des architectures de plusieurs centaines de nœuds de calcul. Les principes de conception sont déjà en place ([ARCHITECTURE.md §10](ARCHITECTURE.md#10-préparation-au-parallélisme-massif)). Chaque sous-étape commence par un ADR.

1. **M9.1 — Passage à l'échelle séquentiel** : stockage de la grille à 2 bits par case, indices compressés si les mesures le justifient, formats binaires `.ngp` et `.ngg`, commandes `convert` et `generate` ([ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md)), corpus massif généré.
2. **M9.2 — Parallélisme en mémoire partagée** : propagation parallèle par lignes (les lignes indépendantes d'une même vague sont traitées en parallèle), avec un espace de travail par thread. Le choix de la technologie (`<threads.h>`, pthreads ou OpenMP) fera l'objet d'un ADR. Il faudra garantir le déterminisme des résultats et des statistiques.
3. **M9.3 — Parallélisme distribué** : partitionnement de la grille et des lignes entre nœuds de calcul, échange des lignes modifiées, et parallélisation de la recherche (partage de l'arbre, vol de travail). Le choix de la technologie (probablement MPI) et de la stratégie de partitionnement fera l'objet d'ADR. Cette étape s'accompagne d'un module optionnel `ng_parallel`, qui n'impose pas MPI aux autres couches.

**Critères de sortie (provisoires)** : courbes d'accélération (*strong scaling* et *weak scaling*) publiées pour 1, 2, 4… N nœuds de calcul. Les résultats sont identiques à ceux du solver séquentiel de référence sur tout le corpus commun.

---

## Questions ouvertes

| # | Question | Impact | Proposition |
|---|---|---|---|
| Q1 | Quelle licence ? | publication, dépendances | MIT, compatible avec Criterion (MIT) |
| Q2 | Hébergement : GitHub ou autre ? | CI | GitHub Actions est supposé dans la documentation |
| Q3 | Quelle source pour le corpus de benchmark ? | M4 | Grilles générées et grilles faites main ; les puzzles publics ne sont importés qu'après vérification de leur licence |
| ~~Q4~~ | ~~Quelle version d'Ubuntu dans WSL ?~~ | — | **Résolu** : Ubuntu 24.04 LTS (déjà installé), avec `gcc-14` et `clang-20` |
| Q5 | Faut-il une API de rappel (*callback*) de progression ou d'annulation ? | M5 et suivantes | à décider avant M5 ; indispensable pour les très longues résolutions |
| Q6 | Quelle architecture de calcul cible : nombre et mémoire des nœuds, réseau d'interconnexion, ordonnanceur (Slurm ?), implémentation MPI disponible ? | M9 | à collecter avant M9.3 ; détermine la taille des grilles du corpus massif |
| Q7 | Quelle machine pour les tests massifs nocturnes (G12) ? | M1 | une machine Linux d'au moins 64 Go de RAM, avec un exécuteur CI auto-hébergé |
