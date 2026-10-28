# AGENTS.md — Instructions pour les agents IA

Ce fichier s'adresse à tout agent IA (Claude, Codex, Copilot, etc.) qui travaille sur ce dépôt. Il est **normatif** : en cas de conflit avec une habitude ou une préférence de l'agent, ce fichier prévaut. En cas de conflit avec une demande explicite d'un humain responsable du projet, l'agent signale le conflit et demande confirmation avant d'agir.

---

## 1. Contexte en 30 secondes

- **Projet** : moteur de résolution de nonogrammes en **C23**, pour Linux (Ubuntu sous WSL2, et le conteneur de référence pour la CI), avec une bibliothèque `libnonogram` (préfixe `ng_`) et un exécutable `nonogram`. Les tests utilisent **Criterion**.
- **Priorités, dans l'ordre** : justesse > lisibilité et maintenabilité > maîtrise de la mémoire > vitesse.
- **Principe central** : toute solution est contrôlée par le vérificateur indépendant (`src/verify/`). Ce vérificateur est l'oracle des tests. Il ne doit jamais dépendre d'un solver.
- **Passage à l'échelle** : les grilles peuvent être massives (plus de 10¹⁰ cases) et certains solvers seront parallélisés sur des centaines de nœuds de calcul. Utilise toujours les types de `types.h` (`ng_dim`, `ng_cell_index`, …) et l'accès par lignes ([ADR-0009](docs/adr/0009-types-et-grilles-massives.md)).
- **Développement itératif** : les étapes M0 à M9 sont décrites dans [docs/ROADMAP.md](docs/ROADMAP.md). On ne commence pas une étape tant que la précédente n'a pas rempli ses critères de sortie.

## 2. Documents à lire avant de travailler

Lis-les dans cet ordre. Ne lis que ceux qui concernent ta tâche, mais **les deux premiers sont toujours obligatoires**.

1. [docs/CODING_STANDARDS.md](docs/CODING_STANDARDS.md) : règles de code obligatoires.
2. [docs/TESTING.md](docs/TESTING.md) : ce que « validé » veut dire.
3. [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) : modules, dépendances autorisées, API.
4. [docs/ROADMAP.md](docs/ROADMAP.md) : étape en cours et périmètre.
5. [docs/adr/](docs/adr/README.md) : décisions déjà prises (ne pas les remettre en cause sans nouvel ADR).
6. [docs/FILE_FORMATS.md](docs/FILE_FORMATS.md) : pour tout travail sur `src/io/` ou les données de test.
7. [docs/agents/PLAYBOOKS.md](docs/agents/PLAYBOOKS.md) : procédures pas à pas pour les tâches courantes.
8. [docs/agents/REVIEW.md](docs/agents/REVIEW.md) : politique de revue indépendante des PR, pour l'auteur comme pour le relecteur.

## 3. Règles d'or

1. **Aucun changement sans test.** Toute modification de comportement s'accompagne d'un test qui échoue avant le changement et réussit après.
2. **Ne jamais affaiblir un test pour le faire passer.** Il est interdit de supprimer une assertion, d'augmenter une tolérance, de désactiver un test, d'ajouter une suppression de sanitizer ou de baisser un seuil de couverture sans accord humain explicite et tracé dans la PR (un accord donné dans la session de travail doit être tracé selon [docs/agents/REVIEW.md §5.1](docs/agents/REVIEW.md#51-accord-humain-donné-dans-la-session-de-travail)).
3. **Respecter les couches.** Les dépendances entre modules sont fixées dans [docs/ARCHITECTURE.md §3](docs/ARCHITECTURE.md#3-modules-et-dépendances). Un solver n'inclut jamais `verify.h`. Le cœur (`core`) n'inclut rien du projet hors de lui-même.
4. **Mémoire explicite.** Toute allocation passe par `ng_allocator`. Aucune allocation dans la boucle de résolution d'un solver. Chaque `*_create` a son `*_destroy`. Voir l'[ADR-0004](docs/adr/0004-gestion-memoire.md).
5. **Pas d'état global mutable**, ni de `exit()`, `abort()` ou `printf()` dans la bibliothèque. Les erreurs remontent sous forme de `ng_status`.
6. **Pas de débordement entier.** Tout calcul de taille passe par `ckd_add` et `ckd_mul`, et le produit de deux `ng_dim` se calcule en `ng_cell_index`. Ne suppose jamais qu'une grille tient dans 2³² cases.
7. **Pas de nouvelle dépendance externe** (bibliothèque, outil obligatoire) sans ADR accepté par un humain.
8. **Ne pas modifier l'API publique** (`include/nonogram/`) sans le signaler explicitement dans la PR et mettre à jour la documentation de l'API.
9. **Des changements petits et ciblés.** Une PR correspond à un objectif. Pas de reformatage ni de refactoring opportuniste hors du périmètre demandé.
10. **Ne jamais pousser sur `main`.** Travaille sur une branche `<type>/<sujet-court>` et ouvre une PR.
11. **La documentation suit le code.** Si tu changes un comportement documenté, mets à jour le document concerné et le `CHANGELOG.md` dans la même PR.

## 4. Commandes

```sh
cmake --preset dev && cmake --build --preset dev     # compiler (GCC, Debug, warnings stricts)
ctest --preset dev                                   # tests unitaires et d'intégration
ctest --preset dev -R verify                         # un sous-ensemble de tests (par nom CTest)
./build/dev/tests/unit/test_grid --filter 'grid_create/*'   # un sous-ensemble de tests Criterion
cmake --preset asan && cmake --build --preset asan && ctest --preset asan   # sanitizers
./scripts/check.sh                                   # TOUTES les portes de qualité (obligatoire avant PR)
./scripts/format.sh                                  # appliquer clang-format
```

`./scripts/check.sh` doit se terminer avec le code 0 avant toute demande de revue. L'environnement attendu est Ubuntu (WSL2) ou le conteneur `tools/docker/Dockerfile`. Si une porte ne peut pas tourner dans ton environnement (outil absent, machine insuffisante pour les tests massifs), dis-le explicitement dans ton rapport. Ne prétends jamais qu'elle a réussi.

## 5. Flux de travail d'une tâche

1. **Comprendre.** Reformule la tâche, identifie l'étape de la feuille de route et les modules touchés. Si la spécification est ambiguë, arrête-toi et pose la question (voir §6).
2. **Planifier.** Liste les fichiers à créer ou modifier, les tests à écrire et l'impact sur l'API ou la mémoire.
3. **Écrire les tests d'abord** : tests unitaires et, si pertinent, tests de propriétés ou d'intégration.
4. **Implémenter** le minimum qui fait passer les tests, en respectant les normes.
5. **Valider** avec `./scripts/check.sh`.
6. **S'auto-relire** avec la liste du §7.
7. **Documenter** : commentaires Doxygen de l'API, documents concernés, `CHANGELOG.md` (section `[Unreleased]`).
8. **Rendre compte** au format du §8.
9. **Faire relire** : une fois la PR ouverte et la CI verte, demander une revue indépendante selon [docs/agents/REVIEW.md](docs/agents/REVIEW.md), puis répondre à chaque constat bloquant, majeur ou question avant de solliciter la revue humaine.

## 6. Quand s'arrêter et demander

Arrête-toi et demande à un humain si :

- la tâche impose de modifier un ADR accepté ou d'en créer un ;
- l'API publique doit changer de façon incompatible ;
- un test existant semble faux (signale-le, ne le corrige pas silencieusement) ;
- un benchmark régresse de plus de 10 % en temps ou en mémoire ;
- la tâche sort du périmètre de l'étape en cours ;
- la spécification autorise plusieurs interprétations raisonnables.

## 7. Liste d'auto-relecture

- [ ] Compile sans warning avec GCC et Clang en `-std=c23 -Werror`.
- [ ] Tous les tests passent, y compris sous ASan et UBSan.
- [ ] Aucune fuite mémoire (sortie ASan/LSan ou Valgrind propre).
- [ ] Chaque chemin d'erreur est testé, y compris l'échec d'allocation (allocateur défaillant).
- [ ] Les entrées des fonctions publiques sont validées (pointeurs, dimensions, débordements de taille via `ckd_*`).
- [ ] Les types sémantiques (`ng_dim`, `ng_cell_index`, …) sont utilisés, il n'y a pas de `int` pour une taille ou un index, et le code reste correct au-delà de 2³² cases.
- [ ] Il n'y a pas de nombre magique, de VLA, de récursion non bornée ni de fonction interdite (voir les normes).
- [ ] Les fonctions publiques ont un commentaire Doxygen qui précise la propriété mémoire et les erreurs possibles.
- [ ] La couverture du code modifié respecte les seuils de [docs/TESTING.md](docs/TESTING.md).
- [ ] `clang-format` et `clang-tidy` ne signalent rien.
- [ ] Le `CHANGELOG.md` et la documentation sont à jour.

## 8. Format du rapport de fin de tâche

```markdown
## Résumé
<1 à 3 phrases : ce qui a été fait et pourquoi>

## Changements
- `chemin/fichier.c` : <nature du changement>

## Validation
- check.sh : OK / KO (détail)
- Portes non exécutées : <liste et raison>, ou « aucune »
- Couverture du code modifié : <x %>

## Impacts
- API publique : aucun / <détail>
- Mémoire et performances : aucun / <mesures>

## Points ouverts
- <questions, dettes introduites, suites proposées>
```

## 9. Conventions rapides

- **Langue** : le code, les identifiants, les commentaires et les messages de commit sont en **anglais**. La documentation dans `docs/` est en **français** ([ADR-0008](docs/adr/0008-langues.md)).
- **Commits** : [Conventional Commits](https://www.conventionalcommits.org/), par exemple `feat(verify): report first mismatching line`.
- **Nommage** : `ng_` pour le public, `ngi_` pour l'interne partagé entre fichiers, `NG_` pour les macros et les constantes.
- **C23** : `nullptr`, `[[nodiscard]]`, `constexpr`, énumérations typées. Pas de `auto`, ni de `_BitInt` (voir [CODING_STANDARDS.md §1](docs/CODING_STANDARDS.md#1-langage-et-compilation)).
- **Tests** : `tests/unit/test_<module>.c`, avec des tests Criterion `Test(<module>_<unité>, <scénario>_<attendu>)` qui utilisent la fixture commune de détection des fuites.
- **Aléatoire** : toute graine est fixée, journalisée et rejouable via `NG_TEST_SEED`.

## 10. Carte du dépôt

Voir [docs/ARCHITECTURE.md §2](docs/ARCHITECTURE.md#2-arborescence). En résumé : `include/nonogram/` pour l'API publique, `src/<module>/` pour l'implémentation, `tests/` pour les tests, `bench/` pour les benchmarks et `docs/` pour la documentation.
