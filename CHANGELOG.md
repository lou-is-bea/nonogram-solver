# Changelog

Toutes les modifications notables de ce projet sont consignées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet adhère au [versionnage sémantique](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- M4 : harnais de benchmark, bibliothèque `ng_bench` (`src/bench/`) et sous-commande `nonogram bench` : exécutions de chauffe puis mesurées (2 et 11 par défaut), temps médian, minimum et maximum sur l'horloge monotone POSIX, pic mémoire par un allocateur compteur, grilles `SOLVED` vérifiées et exécutions contrôlées identiques ; sortie texte ou JSON `nonogram-bench/1` ; corpus figé `bench/corpus/tiny` (10 puzzles) et première référence `bench/baselines/ryzen-9800x3d-wsl.json` ; aides de sortie partagées entre `solve` et `bench` (`src/cli/cli_output.c`) ; F-40 inclut `bench`.
- M4 : sous-commandes `nonogram solve` (choix du solver, comptage, limites de nœuds, de temps et de mémoire, `--output`, `--stats`, sortie JSON `nonogram-solve/1` avec `count_exact`, solution vérifiée avant affichage) et `nonogram list-solvers` ; analyse des options partagée entre `show` et `solve` (`src/cli/cli_args.c`) ; les commandes forment la bibliothèque `nonogram_cli`, testée avec des solvers factices ; comportement de `--output` en cas d'échec d'écriture documenté ; dans les tests d'intégration, les durées sont remplacées par `<ns>` avant comparaison.
- M4 : solver S0, force brute (`brute`, `src/solvers/s0_brute/`), premier solver du registre : énumération binaire des cases inconnues, au plus 25 cases, comptage de solutions, limites de nœuds, de temps et de mémoire ; tests unitaires, propriétés P-S1, P-S4 et P-S5, et test exhaustif du corpus (nouveau label `exhaustive`, lancé par G3, hors Valgrind) ; trois puzzles à solutions multiples d'au plus 25 cases ajoutés au corpus.
- M4 : infrastructure des solvers, nouvel en-tête public `solver.h` : interface `ng_solver_vtable`, registre (`ng_solver_find`, `ng_solver_count`, `ng_solver_at`), `ng_solve` (espace de travail alloué par l'allocateur injectable, résultat et statistiques dans `ng_solve_outcome`, refus au-delà de `max_memory_bytes`), options par défaut et horloge injectable `ng_clock` ; module `ng_solvers` devenu une bibliothèque statique.
- M4 : spécification du solver S0, force brute (`docs/solvers/s0_brute.md`), approuvée par le responsable du projet le 2026-10-28 ; le critère de sortie de M4 sur le comptage porte désormais sur les puzzles à solutions multiples d'au plus 25 cases (`docs/ROADMAP.md`).
- Politique de revue indépendante des PR (`docs/agents/REVIEW.md`) et agent relecteur Claude (`.claude/agents/pr-reviewer.md`) ; étape de revue indépendante dans AGENTS, CONTRIBUTING et le gabarit de PR. Un accord humain donné dans la session de travail et relayé dans la PR par l'agent (citation mot pour mot, date, périmètre, SHA de tête) vaut accord tracé (REVIEW §5.1). Un verdict APPROUVÉE est nécessaire à toute fusion ; un nouveau commit fait tomber le verdict et l'accord de fusion, et les autres accords pour ce qu'il modifie ; un rebasage prouvé sans changement de contenu les conserve (REVIEW §5.2).

## [0.1.0] - 2026-10-27

Première version : modèle de données pour grilles massives, lecture et écriture des formats `.non` et `.sol` en flux, affichage texte, vérificateur indépendant et commandes `nonogram show` et `nonogram verify`. Fin de l'étape M3.

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
- M1 : corpus `tests/data` (12 `tiny` et 6 `small` à solution unique avec leurs `.sol`, 5 `multi` avec leur nombre de solutions, 1 `unsolvable`, 16 `invalid` avec l'erreur attendue), dessiné pour le projet, provenance dans `tests/data/README.md`, vérifié par `test_corpus`.
- M1 : cible de fuzzing `fuzz_non_parser` (aller-retour et absence de fuite vérifiés sur chaque entrée), dictionnaire `.non`, porte G9 active.
- M1 : premiers tests massifs (10⁴ × 10⁴ : formules mémoire à ± 5 %, accès par ligne, allers-retours `.non` et `.sol` ; limite 10⁵ × 10⁵ avec l'allocateur plafonné), label `massive` et preset `massive`.
- Paquet `libstdc++-14-dev` ajouté à l'environnement de référence (requis par libFuzzer avec Clang et GCC 14).
- M2 : `render.h` : `ng_render_text` (ASCII ou Unicode, avec ou sans indices, grille complète, partielle ou absente) et `ng_render_summary` ; module `ng_render`.
- M2 : sous-commande `nonogram show <puzzle> [--solution <sol>] [--unicode] [--max-render <n>]`, aide `--help`, codes de sortie stables (F-41).
- M2 : tests d'intégration de la CLI comparés à des sorties de référence (`tests/integration/`, `tests/data/expected/`), script `scripts/update_golden.sh`.
- M3 : `verify.h` : vérificateur indépendant `ng_verify` (verdicts `VALID`, `INCOMPLETE`, `INVALID`, première ligne et premier bloc fautifs, nombre de cases inconnues) et `ng_verify_line_runs` (blocs observés d'une ligne) ; module `ng_verify`.
- M3 : sous-commande `nonogram verify <puzzle> <grille>` (sortie `valid`, `incomplete: …` ou `invalid: row 2 has runs 1 2, expected 1 1`, codes de sortie 0 à 4 selon F-41 ; une grille de mauvaises dimensions est une erreur d'entrée, code 3).
- M3 : propriétés P-V1 à P-V4 (et cohérence des blocs observés), nombre de cas réglable par `NG_PROPERTY_CASES` ; test massif du vérificateur sur 10⁸ cases ; grilles fausses de test (`tests/data/grids/`).

### Modifié
- Troisième revue indépendante de M3 : cas `verify_extra_runs` (ligne qui a plus de blocs que son indice, détecté sous ASan si les tampons sont sous-dimensionnés) ; la règle ADR-0007 de G2 accepte tout préfixe de chemin ; test d'échec d'écriture de `verify` rangé avec les autres ; exemple du README corrigé.
- M2 close : les 35 sorties de référence de la CLI sont approuvées par le responsable du projet (PR #3).
- Troisième revue indépendante de M2 : test d'échec d'écriture de `--help` (code 3) ; `--version` testé sous `NG_CLI_WRAPPER` ; motif de `large/stripes_100.non` décrit dans le fichier ; grille inutilisée `grids/stripes_100_solution.sol` retirée.
- Seconde revue indépendante de M3 : tailles des tampons de `nonogram verify` calculées avec `ckd_*` (règle d'or 6) ; cas d'un indice plus long que les blocs observés, d'une seule case inconnue (« 1 unknown cell »), d'une option en deuxième position et d'un échec d'écriture ; la règle ADR-0007 de G2 couvre aussi `nonogram/nonogram.h`, les `<…>` et les chemins relatifs.
- Seconde revue indépendante de M2 : test d'échec d'écriture avec une sortie plus grande que les tampons (puzzle `large/stripes_100.non`), refus d'un `--max-render` répété testé, G2 interdit toute allocation dans `src/render/`, `--help` et `--version` vérifient leur écriture, G7 affiche le diagnostic d'un cas qui échoue sous Valgrind. M2 repasse « en cours » jusqu'à l'approbation des sorties de référence.
- Revue indépendante de M3 : tests de `ng_verify` avec une seule dimension différente et sur une colonne de 10⁶ cases ; cas CLI `verify` d'un indice vide et d'une hauteur seule différente ; message d'erreur interne distinct du manque de mémoire ; G2 interdit à `verify` d'inclure un solver et inversement (ADR-0007).
- Revue indépendante de M2 : `ng_render_options` devient `{ hide_clues, unicode }`, si bien qu'une valeur nulle et `nullptr` donnent le même rendu par défaut (indices, ASCII) ; l'exemple de `render.h` est corrigé. `nonogram show` refuse une option répétée et une option à la place d'une valeur. Nouveaux tests : une seule dimension différente (rendu, résumé, chargement), bornes de `--max-render`, `--solutionfoo`, échec d'écriture (code 3), absence d'allocation du rendu, grilles 1 × 1000 et 1000 × 1. G7 exécute aussi les cas d'intégration de la CLI sous Valgrind.
- Revue indépendante de M1 : les accesseurs sans statut (`ng_grid_get`, `ng_grid_set`, `ng_grid_fill`, `ng_puzzle_line_length`, `ng_puzzle_line_run_count`) n'ont plus d'assertion sur les entrées de l'appelant ; leur valeur de repli documentée s'applique dans tous les builds (CODING_STANDARDS §6).
- Revue indépendante de M1 : le fuzzer et le test massif d'aller-retour `.non` comparent les longueurs des blocs, plus seulement leur nombre ; le pic mémoire documenté des lecteurs est corrigé (jusqu'à environ 3 fois les indices ou la grille) ; un `\r` isolé est refusé aussi dans un commentaire ; l'analyseur `.non` est découpé en trois fichiers (`non_lexer.c`, `non_directives.c`, `non_parser.c`).
- clang-tidy, avec l'accord du responsable du projet (2026-10-25) : `readability-implicit-bool-conversion` désactivé (inadapté au C) ; `readability-magic-numbers` et `readability-function-cognitive-complexity` désactivés pour `tests/` seulement (`tests/.clang-tidy`). CODING_STANDARDS documente ces exceptions et autorise les attributs de fabricant qui n'ajoutent que des diagnostics (`[[gnu::format]]`).
- `ngi_output` (sortie texte tamponnée) déplacé de `src/io/internal` vers `src/core/internal`, pour être partagé par `io` et `render` sans dépendance entre ces couches.
- FILE_FORMATS : règles strictes précisées là où la spécification était muette (directives répétées, valeurs entre guillemets, contenu entre les sections, fin de ligne finale facultative, `\r\n` accepté pour `.sol`).
- `ng_grid_load_line` et `ng_grid_store_line` ne prennent pas le puzzle en paramètre, conformément à l'ADR-0009 (le croquis d'ARCHITECTURE §4.4 et §5 est corrigé).
- M0 : la CI GitHub Actions, d'abord différée (validation locale avec `scripts/check.sh`), tourne sur chaque PR ; les tâches de test ont reçu la permission `packages: read` pour télécharger l'image de référence ; une tâche `full-check` exécute `check.sh` en entier dans le conteneur. Le dépôt est public et `main` est protégée.
- Revue indépendante de M0 : test d'usage de la CLI qui vérifie exactement le code 2 ; recherche des constructions interdites qui retire les commentaires (au lieu d'ignorer toute ligne commençant par `*`), avec un auto-test (littéraux de chaîne et de caractère neutralisés, y compris prolongés par `\`, séparateurs de chiffres C23 reconnus), et qui interdit aussi `strdup`/`strndup` ; `noTestsAction` à `error` ; compilation en `-std=c23`.
- ADR-0001 : passage de C11 à C23 (GCC ≥ 14, Clang ≥ 19), plateforme Linux uniquement.
- ADR-0003 : Criterion remplace Unity comme framework de tests.
- Retrait de toute cible Windows et MSVC (développement sous Ubuntu dans WSL2).
- Environnement de référence : Ubuntu 24.04 LTS avec GCC 14 et Clang 20 (paquets versionnés, compilateurs par défaut non utilisés).
