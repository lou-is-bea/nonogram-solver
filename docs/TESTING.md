# Stratégie de test et pipeline de validation

> Dernière revue : 2026-10-25. Ce document est **normatif** : il définit ce que veut dire « validé ».

---

## 1. Principes

1. **Aucun développement n'est intégré sans validation.** La pipeline complète passe sur chaque PR, et une revue humaine est obligatoire.
2. **Le vérificateur est l'oracle.** Toute grille produite par un solver dans un test passe par `ng_verify`. Le vérificateur est lui-même le module le plus testé du projet.
3. **On teste d'abord.** Une correction de bug commence par un test qui reproduit le bug.
4. **Les tests sont déterministes.** Les graines sont fixées et affichées, le résultat ne dépend jamais de l'heure, et un test n'est jamais « instable » : un test instable est un bug à corriger immédiatement.
5. **Les chemins d'erreur sont des chemins comme les autres.** Ils sont testés, y compris l'échec d'allocation et le dépassement des limites de taille.

## 2. Niveaux de test

| Niveau | Emplacement | Outil | Ce qui est testé |
|---|---|---|---|
| Unitaire | `tests/unit/` | Criterion et CTest | une fonction ou un module isolé |
| Propriétés et différentiel | `tests/property/` | Criterion et le générateur avec graine | des invariants sur des milliers d'entrées aléatoires, et l'accord entre solvers |
| Intégration (CLI) | `tests/integration/` | CTest et un script de comparaison | l'exécutable de bout en bout, comparé à des sorties de référence |
| Grilles massives | `tests/massive/` | Criterion, label `massive` | le passage à l'échelle des types, des formats et des solvers |
| Fuzzing | `tests/fuzz/` | libFuzzer (Clang) | la robustesse des analyseurs face à des entrées arbitraires |
| Mémoire | toute la suite | ASan, UBSan, LSan, Valgrind | fuites, accès hors limites, comportement indéfini, débordements entiers |
| Performance | `bench/` | le harnais maison | temps médian et pic mémoire, comparés aux références |

## 3. Portes de qualité

Une porte **bloquante** qui échoue empêche la fusion.

| # | Porte | Commande (via `check.sh`) | Bloquante | À partir de |
|---|---|---|---|---|
| G0 | Formatage | `clang-format --dry-run --Werror` | oui | M0 |
| G1 | Compilation sans warning (GCC et Clang) | presets `dev` (GCC) et `dev-clang`, avec `-Werror` | oui | M0 |
| G2 | Analyse statique | `clang-tidy`, `cppcheck --error-exitcode=1`, recherche des fonctions interdites | oui | M0 |
| G3 | Tests unitaires | `ctest --preset dev -L unit` | oui | M0 |
| G4 | Tests d'intégration | `ctest --preset dev -L integration` | oui | M2 |
| G5 | Propriétés et différentiel | `ctest --preset dev -L property` | oui | M3 |
| G6 | Sanitizers | `ctest --preset asan` (ASan et LSan), `--preset ubsan` (UBSan, avec `-fsanitize=integer` sous Clang) | oui | M0 |
| G7 | Valgrind memcheck | `ctest -T memcheck` (preset `dev`), avec `--trace-children=yes` | oui | M1 |
| G8 | Couverture | preset `coverage` et `gcovr`, avec les seuils du §6 | oui | M1 |
| G9 | Fuzzing rapide | 60 s par cible | oui | M1 |
| G10 | Benchmarks | `bench_compare.py` comparé à la référence de `main` | oui si plus de 10 %, sinon simple rapport | M4 |
| G11 | Fuzzing long | 1 h par cible | non (nocturne) | M1 |
| G12 | Tests massifs | `ctest --preset release -L massive` | non (nocturne, machine dédiée) | M1 |

`./scripts/check.sh` exécute G0 à G9 en local, dans cet ordre, et s'arrête à la première porte qui échoue. `./scripts/check.sh --quick` n'exécute que G0 à G3, pour itérer plus vite, et `./scripts/check.sh --only G0,G2` n'exécute que les portes demandées (c'est ce qu'utilise la CI). Avant une PR, c'est la version complète qui fait foi. Une porte qui n'est pas encore active à l'étape en cours est signalée `SKIP` et rappelée en fin d'exécution : elle n'est jamais présentée comme réussie.

## 4. Détail par niveau

### 4.1 Conventions des tests unitaires (Criterion)

- Un fichier par module : `tests/unit/test_<module>.c`. Chaque fichier produit un exécutable enregistré dans CTest avec le label `unit`.
- Les tests se nomment `Test(<module>_<unité>, <scénario>_<attendu>)`, par exemple `Test(grid_create, zero_width_returns_limit)`.
- Chaque test suit la structure **Préparer, Agir, Vérifier**, et ne vérifie qu'un comportement.
- Les tests sont indépendants. Criterion exécute chaque test dans un processus séparé, mais un test ne doit pas pour autant dépendre de cette isolation.
- Chaque suite déclare la fixture commune `ng_test_fixture` (`.init` et `.fini`, définie dans `tests/support/`). Elle installe un allocateur compteur et **fait échouer le test s'il laisse une fuite**.
- On utilise les attributs Criterion à bon escient :
  - `.timeout` sur tout test de solver (par défaut 10 s), pour qu'une boucle infinie apparaisse comme un échec ;
  - `.signal = SIGABRT` pour vérifier qu'une assertion interne se déclenche en Debug ;
  - `ParameterizedTest` pour les tableaux de cas limites et l'itération sur les fichiers du corpus.
- On exécute un sous-ensemble avec `./build/dev/tests/unit/test_grid --filter 'grid_create/*'`, ou avec `ctest --preset dev -R grid`.

### 4.2 Cas limites obligatoires

Toute fonction qui traite des lignes ou des grilles est testée au minimum sur ces cas :

- une grille 1×1, vide puis pleine ;
- une ligne vide (indice `0`), une ligne entièrement pleine (un indice égal à la longueur) ;
- des indices qui laissent exactement un espace entre les blocs (aucune liberté de placement) ;
- une grille non carrée (largeur différente de la hauteur, dans les deux sens), y compris très allongée (1 × 10⁶ et 10⁶ × 1) ;
- une dimension égale à `NG_DIM_MAX` et une égale à `NG_DIM_MAX + 1` (refusée avec `NG_ERR_LIMIT`). Ces tests utilisent l'**allocateur plafonné**, qui refuse proprement l'allocation : on vérifie le calcul de taille et les chemins d'erreur sans allouer réellement des gigaoctets ;
- des dimensions dont le produit dépasse 2³² cases (par exemple 70 000 × 70 000), pour détecter les calculs faits par erreur en 32 bits (calcul de taille vérifié avec l'allocateur plafonné) ;
- un puzzle à solutions multiples et un puzzle insoluble.

### 4.3 Échecs d'allocation

`tests/support/failing_allocator` fait échouer la n-ième allocation. Pour chaque constructeur et chaque fonction qui alloue :

```c
Test(grid_create, every_allocation_failure_is_clean, .init = ng_test_setup, .fini = ng_test_teardown)
{
    for (size_t n = 0; ; ++n) {
        failing_allocator_arm(&fa, n);                     /* échoue à la n-ième allocation */
        ng_grid *g = nullptr;
        ng_status s = ng_grid_create(&fa.base, 7, 5, &g);
        if (s == NG_OK) {                                  /* toutes les allocations ont réussi : fin */
            ng_grid_destroy(g);
            cr_assert_eq(fa.live_bytes, 0);
            break;
        }
        cr_assert_eq(s, NG_ERR_OUT_OF_MEMORY);
        cr_assert_null(g);
        cr_assert_eq(fa.live_bytes, 0);                    /* aucune fuite sur le chemin d'erreur */
    }
}
```

### 4.4 Tests de propriétés

Le générateur produit une grille aléatoire `G` (dimensions et densité tirées au sort), à partir d'une graine affichée en cas d'échec. Pour rejouer un cas : `NG_TEST_SEED=<graine> ctest --preset dev -R <test>`. Les théories Criterion (`Theory`) peuvent servir pour les petits domaines énumérables. Les propriétés à grand volume utilisent notre générateur, pour garder le contrôle de la graine.

**Vérificateur (M3)**

| Propriété | Énoncé |
|---|---|
| P-V1 | `verify(clues(G), G) == VALID` |
| P-V2 | inverser une seule case de `G` donne `INVALID` (le nombre de cases pleines de la rangée change) |
| P-V3 | passer une case de `G` à `UNKNOWN` donne `INCOMPLETE` |
| P-V4 | `verify` sur une transposition : `verify(transpose(clues(G)), transpose(G)) == VALID` |

**Modèle et analyseurs (M1)**

| Propriété | Énoncé |
|---|---|
| P-M1 | `store_line` puis `load_line` redonne la ligne écrite, pour toute ligne (rangée ou colonne) |
| P-IO1 | aller-retour `.sol` : `read(write(G)) == G` |
| P-IO2 | aller-retour `.non` : `read(write(P))` redonne les mêmes indices que `P` |
| P-IO3 | la lecture en flux donne le même résultat quelle que soit la taille des blocs lus (1 octet, 7 octets, 64 Kio) |

**Solvers (M4 et suivantes)**

| Propriété | Énoncé |
|---|---|
| P-S1 | soit `P = clues(G)`. Un solver complet renvoie `SOLVED`, et `verify(P, résultat) == VALID` (le résultat peut différer de `G` si la solution n'est pas unique) |
| P-S2 | correction de la propagation : toute case déduite par S2 vaut la même chose dans toutes les solutions énumérées par S0 |
| P-S3 | test différentiel : sur le corpus `tiny`, tous les solvers complets donnent le même résultat et le même nombre de solutions (avec un plafond) |
| P-S4 | monotonie : une case connue en entrée n'est jamais modifiée |
| P-S5 | déterminisme : deux exécutions avec la même graine donnent des statistiques identiques |
| P-S6 | aucune allocation : l'allocateur compteur ne voit aucune allocation pendant `solve()` |

Volume : 1 000 cas par propriété sur chaque PR, 100 000 en nocturne.

### 4.5 Tests d'intégration (CLI)

- Chaque cas est un triplet (commande, fichier d'entrée, sortie attendue avec le code de sortie) décrit dans `tests/integration/cases.cmake`.
- Les sorties de référence (`tests/data/expected/`) sont **relues par un humain** lors de leur création. Une mise à jour en masse (`scripts/update_golden.sh`) se justifie dans la PR, et chaque modification se relit au diff.

### 4.6 Grilles massives

- Les grilles sont **générées** à partir de paramètres versionnés (dimensions, densité, graine), jamais stockées dans le dépôt.
- Paliers : 10⁴ × 10⁴ (accessible à une machine de développement), 10⁵ × 10⁵ (machine dédiée), puis plus grand avec les solvers distribués (M9).
- On vérifie : aucun débordement (build UBSan avec `-fsanitize=integer`), la mémoire mesurée conforme à la formule documentée (à ±5 %), la cohérence des allers-retours d'entrée-sortie, et les solvers de propagation ou complets dans les limites prévues par leur spécification.
- Ces tests sont lancés en nocturne (G12) et à la demande. Le dimensionnement de la machine est noté dans `tests/massive/README.md`.

### 4.7 Fuzzing

- Les cibles sont `fuzz_non_parser`, `fuzz_sol_parser`, puis, à partir de M6, `fuzz_line_solver` (indices et ligne partielle aléatoires, comparés à l'énumération naïve), et à M9 `fuzz_ngp_parser` et `fuzz_ngg_parser`.
- Le corpus initial est `tests/data/puzzles/**`. Les plantages trouvés sont ajoutés comme tests de non-régression dans `tests/data/regressions/`.

### 4.8 Benchmarks

- Le corpus `bench/corpus/{small,medium,large,hard}` contient des puzzles figés et versionnés. Le corpus `bench/corpus/massive.json` ne contient que les paramètres de génération et l'empreinte attendue des fichiers.
- Pour chaque couple (solver, puzzle), le harnais fait 2 exécutions de chauffe, puis N = 11 exécutions mesurées (N = 3 pour le corpus massif). Il retient le temps **médian** (horloge monotone) et le pic mémoire (allocateur compteur), et écrit les résultats au format JSON.
- `bench/baselines/<machine>.json` sert de référence. Elle n'est mise à jour qu'en fusionnant une PR qui justifie le changement.
- Les mesures de la CI sont bruitées : elles servent à repérer les tendances. Une régression signalée se confirme en local avant toute décision.

### 4.9 Particularités de Criterion avec les outils mémoire

- **Valgrind** : Criterion exécute les tests dans des processus fils, donc Valgrind est lancé avec `--trace-children=yes --leak-check=full --error-exitcode=1` (configuration dans `CTestCustom.cmake`). Le seul fichier de suppressions autorisé est `tests/valgrind/criterion.supp`, qui ne vise que les fonctions internes de Criterion. Toute suppression qui porterait sur du code du projet est interdite.
- **ASan et LSan** : ils fonctionnent dans les processus fils. LSan signale une fuite à la sortie du processus fils, après que Criterion a enregistré le résultat : l'exécutable de test renvoie donc 0 malgré le rapport. Chaque test CTest déclare `FAIL_REGULAR_EXPRESSION` sur les rapports de sanitizers (`ERROR: AddressSanitizer`, `ERROR: LeakSanitizer`, `runtime error:`) : c'est ce mécanisme qui fait échouer le test concerné. Un résultat sous sanitizers se juge donc toujours via `ctest`.
- **Couverture** : les processus fils écrivent leurs données gcov à leur sortie. Un test qui se termine par un signal (`.signal`) ne contribue pas à la couverture, ce qui est sans conséquence.

## 5. Intégration continue

GitHub Actions, fichier `.github/workflows/ci.yml`. Toutes les tâches s'exécutent dans le **conteneur de référence** construit depuis `tools/docker/Dockerfile` (Ubuntu 24.04 LTS : GCC 14, Clang 20, Criterion, CMake, Ninja, Valgrind, outils d'analyse aux versions figées). C'est le même environnement que celui de WSL.

| Tâche | Compilateur | Portes |
|---|---|---|
| `lint` | Clang | G0, G2 |
| `build-test` (matrice) | GCC 14 et Clang 20 (référence), Clang 19 (version minimale supportée) | G1, G3, G4, G5 |
| `sanitizers` | Clang | G6 |
| `valgrind` | GCC | G7 |
| `coverage` | GCC | G8 (le rapport est publié comme artefact) |
| `fuzz-smoke` | Clang | G9 |
| `bench` | GCC, Release, sur chaque PR à partir de M4 | G10 |
| `nightly` | planifiée, branche `main` | G5 sur 100 000 cas, G11, benchmarks complets |
| `massive` | machine dédiée (exécuteur auto-hébergé), planifiée | G12 |

**Protection de `main`** : toutes les tâches bloquantes doivent être vertes, au moins une approbation humaine est nécessaire, la branche doit être à jour avant fusion, et il est impossible de pousser directement.

## 6. Seuils de couverture

| Module | Lignes | Branches |
|---|---|---|
| `verify` | ≥ 95 % | ≥ 90 % |
| `core`, `io` | ≥ 90 % | ≥ 80 % |
| `render`, `solvers` | ≥ 85 % | ≥ 75 % |
| `cli` | ≥ 75 % | — |

Le code ajouté par une PR ne doit pas faire baisser la couverture du module concerné. La couverture reste un indicateur : un test sans assertion significative est refusé en revue, même s'il fait monter le chiffre.

## 7. Données de test

`tests/data/README.md` recense la **provenance** et la licence de chaque fichier.

```
tests/data/
├── puzzles/
│   ├── tiny/       ≤ 25 cases (accessibles à S0)
│   ├── small/      ≤ 15×15
│   ├── medium/     ≤ 30×30
│   ├── large/      ≤ 100×100
│   ├── multi/      plusieurs solutions (nombre attendu dans le nom : *_n3.non)
│   ├── unsolvable/
│   └── invalid/    fichiers mal formés (message d'erreur attendu dans *.expected)
├── solutions/      <nom>.sol pour chaque puzzle à solution unique
├── expected/       sorties CLI de référence
└── regressions/    cas issus de bugs ou du fuzzing (un fichier par bug, ticket indiqué)
```

## 8. Environnement de référence

- Le développement se fait sous **Ubuntu dans WSL2**, avec les mêmes paquets que l'image `tools/docker/Dockerfile` (liste dans [CONTRIBUTING.md §2](../CONTRIBUTING.md#2-mise-en-place-de-lenvironnement)).
- Toutes les portes G0 à G9 s'exécutent en local, dans WSL. Aucune ne dépend de Windows.
- En cas de doute sur un écart entre la CI et le poste local, on rejoue la vérification dans le conteneur : `docker run --rm -v "$PWD:/src" -w /src nonogram-dev ./scripts/check.sh`.
