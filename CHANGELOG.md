# Changelog

Toutes les modifications notables de ce projet sont consignées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet adhère au [versionnage sémantique](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- M0 : build CMake (C23 strict, vérification des fonctionnalités C23 à la configuration), cibles `ng_core`, `ng_io`, `ng_render`, `ng_verify`, `ng_solvers` et `nonogram`, presets `dev`, `dev-clang`, `release`, `asan`, `ubsan`, `coverage` et `fuzz`.
- M0 : API publique `version.h` (`NG_VERSION_MAJOR`, `NG_VERSION_MINOR`, `NG_VERSION_PATCH`, `ng_version_string()`) et `nonogram --version`.
- M0 : tests Criterion (fixture commune, `test_version`), contrôle d'autonomie des en-têtes publics, tests de fumée de la CLI, échec de tout test dont la sortie contient un rapport de sanitizer.
- M0 : `.clang-format`, `.clang-tidy`, `.editorconfig`, `scripts/check.sh` (portes G0 à G9, options `--quick` et `--only`), `scripts/format.sh`, `tests/README.md`.
- M0 : image de référence `tools/docker/Dockerfile` et CI GitHub Actions (`lint`, `build-test` GCC 14 / Clang 20 / Clang 19, `sanitizers`).
- Documentation initiale du projet : README, AGENTS, CONTRIBUTING, exigences, architecture, feuille de route, normes de code, stratégie de test, formats de fichiers, glossaire, ADR 0001 à 0010 et gabarits.
- ADR-0009 : types sémantiques et support des grilles massives.
- ADR-0010 : lecture en flux et formats binaires `.ngp` / `.ngg`.
- Étape M9 de la feuille de route : grilles massives et parallélisme.
- `scripts/setup-wsl.sh` : installation de l'environnement Ubuntu (WSL2), clonage et import initial.
- `.gitignore` et `.gitattributes` (fins de ligne LF).

- M1 : `types.h` (types sémantiques, `NG_DIM_MAX`, `NG_RUN_COUNT_MAX`, macros `NG_PRI_*`), `status.h` (`ng_status`, `ng_status_str()`), `allocator.h` (`ng_allocator`, `ng_allocator_default()`) et l'en-tête parapluie `nonogram.h`.
- M1 : allocateur de test `ng_test_allocator` (compteur, défaillant, plafonné) et fixture commune qui détecte les fuites et les violations du contrat de l'allocateur.
- M1 : portes G7 (Valgrind, analyse des journaux par processus) et G8 (couverture par module avec `gcovr` et `gcov-14`) actives dans `check.sh`.
- M1 : `stream.h` : `ng_reader` et `ng_writer` (fichier `FILE *`, bloc mémoire avec état `ng_memory_source` fourni par l'appelant), tampon extensible opaque `ng_buffer` (`create`, `destroy`, `data`, `size`) et `ng_writer_from_buffer` (écritures tout ou rien).
- M1 : `puzzle.h` (`ng_puzzle` opaque et immuable, indices au format CSR en une allocation, validation des invariants dont le nombre de blocs par ligne avant toute lecture des blocs) et `grid.h` (`ng_cell`, `ng_grid` opaque à un octet par case, accès par case et par copie de ligne, `ng_grid_copy`, `ng_grid_fill`).
- M1 : assertions internes `NG_ASSERT` et `NG_UNREACHABLE` (`src/core/internal/assert.h`).
- M1 : générateur pseudo-aléatoire de test `ng_test_rng` (xoshiro256**, graine `NG_TEST_SEED`) et premier test de propriété (P-M1) ; la porte G5 est active.
- M1 : G3 exécute aussi les tests unitaires avec le preset `release`.
- M1 : `io.h` : lecture en flux des formats `.non` (`ng_io_read_non`, blocs de 64 Kio, erreurs avec ligne, colonne et message) et `.sol` (`ng_io_read_sol`), écriture canonique (`ng_io_write_non`, `ng_io_write_sol`), métadonnées `.non` dans un objet séparé (`ng_non_metadata`).
- M1 : accès interne en lecture seule aux indices d'une ligne (`ngi_puzzle_line_runs`), pour les couches io et render.
- M1 : propriétés P-IO1 (aller-retour `.sol`), P-IO2 (aller-retour `.non`, forme canonique stable) et P-IO3 (résultat indépendant de la taille des blocs lus) ; supports de test `test_streams` et `test_puzzles`.
- M1 : corpus `tests/data` (12 `tiny` et 6 `small` à solution unique avec leurs `.sol`, 5 `multi` avec leur nombre de solutions, 1 `unsolvable`, 14 `invalid` avec l'erreur attendue), dessiné pour le projet, provenance dans `tests/data/README.md`, vérifié par `test_corpus`.
- M1 : cible de fuzzing `fuzz_non_parser` (aller-retour et absence de fuite vérifiés sur chaque entrée), dictionnaire `.non`, porte G9 active.
- M1 : premiers tests massifs (10⁴ × 10⁴ : formules mémoire à ± 5 %, accès par ligne, allers-retours `.non` et `.sol` ; limite 10⁵ × 10⁵ avec l'allocateur plafonné), label `massive` et preset `massive`.
- Paquet `libstdc++-14-dev` ajouté à l'environnement de référence (requis par libFuzzer avec Clang et GCC 14).

### Modifié
- FILE_FORMATS : règles strictes précisées là où la spécification était muette (directives répétées, valeurs entre guillemets, contenu entre les sections, fin de ligne finale facultative, `\r\n` accepté pour `.sol`).
- `ng_grid_load_line` et `ng_grid_store_line` ne prennent pas le puzzle en paramètre, conformément à l'ADR-0009 (le croquis d'ARCHITECTURE §4.4 et §5 est corrigé).
- M0 : la CI GitHub Actions, d'abord différée (validation locale avec `scripts/check.sh`), tourne sur chaque PR ; les tâches de test ont reçu la permission `packages: read` pour télécharger l'image de référence ; une tâche `full-check` exécute `check.sh` en entier dans le conteneur. Le dépôt est public et `main` est protégée.
- Revue indépendante de M0 : test d'usage de la CLI qui vérifie exactement le code 2 ; recherche des constructions interdites qui retire les commentaires (au lieu d'ignorer toute ligne commençant par `*`), avec un auto-test (littéraux de chaîne et de caractère neutralisés, y compris prolongés par `\`, séparateurs de chiffres C23 reconnus), et qui interdit aussi `strdup`/`strndup` ; `noTestsAction` à `error` ; compilation en `-std=c23`.
- ADR-0001 : passage de C11 à C23 (GCC ≥ 14, Clang ≥ 19), plateforme Linux uniquement.
- ADR-0003 : Criterion remplace Unity comme framework de tests.
- Retrait de toute cible Windows et MSVC (développement sous Ubuntu dans WSL2).
- Environnement de référence : Ubuntu 24.04 LTS avec GCC 14 et Clang 20 (paquets versionnés, compilateurs par défaut non utilisés).
