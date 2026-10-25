# Données de test

Ce dossier contient le corpus de puzzles utilisé par les tests ([TESTING.md §7](../../docs/TESTING.md#7-données-de-test)). Tout fichier ajouté ici doit avoir une provenance et une licence connues ([PLAYBOOKS P4](../../docs/agents/PLAYBOOKS.md#p4--ajouter-des-puzzles-au-corpus)).

## Provenance et licence

| Fichiers | Provenance | Licence |
|---|---|---|
| `puzzles/tiny/*`, `puzzles/small/*`, `puzzles/multi/*`, `puzzles/unsolvable/*`, `solutions/*` | Dessinés à la main pour ce projet (2026-10-25). Les indices sont dérivés du dessin, qui est la solution. | celle du projet (à définir, question Q1 de la [feuille de route](../../docs/ROADMAP.md#questions-ouvertes)) |
| `puzzles/invalid/*`, `grids/*` | Écrits à la main pour ce projet (2026-10-25), un cas d'erreur par fichier. | idem |

Aucun puzzle n'est importé d'une source externe.

## Organisation et conventions

| Dossier | Contenu | Garanties vérifiées |
|---|---|---|
| `puzzles/tiny/` | au plus 25 cases | solution unique, dans `solutions/<nom>.sol` |
| `puzzles/small/` | au plus 15 × 15 | solution unique, dans `solutions/<nom>.sol` |
| `puzzles/large/` | jusqu'à 100 × 100, générés (motif décrit dans le fichier) ; sans garantie d'unicité, ils servent aux tests de la CLI sur de grandes sorties | bien formés |
| `puzzles/multi/` | plusieurs solutions ; leur nombre exact est dans le nom (`*_n9.non`) | nombre de solutions |
| `puzzles/unsolvable/` | indices cohérents (chaque ligne tient, totaux égaux) mais aucune grille ne les satisfait | aucune solution |
| `puzzles/invalid/` | fichiers mal formés ou incohérents, chacun avec un `<nom>.expected` | erreur attendue |
| `grids/` | grilles volontairement fausses (dimensions ou contenu) ou partielles, pour la CLI (`<puzzle>_<défaut>.sol`) | défaut décrit par le nom |
| `expected/` | sorties de référence de la CLI (`<cas>.out`, `<cas>.err`), cas décrits dans `tests/integration/cases.cmake` | relues par un humain ; mises à jour par `scripts/update_golden.sh` |

Un fichier `.expected` contient trois lignes :

```
status NG_ERR_PARSE
position 1:7
message width out of range [1, 2147483647]: 0
```

La position est `ligne:colonne` (base 1) et le message est un fragment qui doit apparaître dans `ng_parse_error.message`.

## Vérifications

- `tests/unit/test_corpus.c` lit chaque puzzle et, pour `tiny` et `small`, vérifie que les indices recalculés depuis la solution sont exactement ceux du puzzle et que les dimensions respectent la catégorie. Pour `invalid`, il vérifie le statut, la position et le message attendus.
- L'unicité (`tiny`, `small`), le nombre de solutions (`multi`) et l'absence de solution (`unsolvable`) ont été vérifiés à la création par une énumération indépendante (placements de lignes avec élagage par colonnes). Ils seront revérifiés par le solver S0 à partir de M4.
- Le corpus sert aussi de corpus initial au fuzzer `fuzz_non_parser`.
