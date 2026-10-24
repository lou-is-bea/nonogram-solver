# ADR-0007 : Vérificateur indépendant utilisé comme oracle

- **Statut** : Accepté
- **Date** : 2026-10-24
- **Exigences liées** : F-20, F-22, F-32

## Contexte

Les solvers vont devenir de plus en plus complexes et optimisés. Il faut un moyen **simple et fiable** de garantir que leurs résultats sont justes, sans dépendre de leur code.

## Options étudiées

- **Chaque solver vérifie lui-même son résultat** : si la logique de vérification partage du code avec celle du solver, un même bug peut fausser les deux.
- **Comparer à des solutions connues** : nécessaire, mais insuffisant (solutions multiples, puzzles générés sans solution de référence).
- **Un vérificateur indépendant et volontairement naïf** : on recalcule les blocs de chaque ligne et on les compare aux indices, en `O(l×h)`.

## Décision

- `src/verify/` est une couche parallèle aux solvers. **Ni l'un ni l'autre ne dépend de l'autre**, et cette règle est imposée par CMake.
- Son implémentation reste **délibérément simple**, sans optimisation qui en compromettrait la lisibilité.
- Chaque grille annoncée `SOLVED`, dans un test, un benchmark ou la CLI, passe par `ng_verify`.
- `verify` vise la couverture la plus élevée du projet (≥ 95 % des lignes) et ses propriétés (P-V1 à P-V4) sont testées.

## Conséquences

- **Positives** : un seul composant à « croire », court et testé à fond. Les bugs des solvers sont détectés quel que soit l'algorithme.
- **Négatives** : la vérification a un coût en `O(l×h)` à chaque résolution, négligeable devant celui de la résolution.
- **Actions** : étape M3, avant tout solver.

## Critères de réexamen

Aucun prévu : c'est un principe fondateur.
