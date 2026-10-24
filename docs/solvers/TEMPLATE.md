# Solver S<N> : <nom>

> Copier ce gabarit vers `docs/solvers/s<N>_<nom>.md`. La spécification doit être **approuvée par un humain avant l'implémentation**, puis mise à jour avec les mesures réelles une fois le solver livré.

- **Nom dans le registre** : `<nom>`
- **Étape** : M<x>
- **Statut** : Spécifié / Implémenté / Optimisé
- **Complet** : oui / non (voir [GLOSSARY.md](../GLOSSARY.md))

## 1. Principe

<L'idée de l'algorithme en quelques phrases, avec une référence bibliographique si elle existe.>

## 2. Algorithme

```
<pseudo-code>
```

## 3. Complexité

| Ressource | Pire cas | Cas typique | Justification |
|---|---|---|---|
| Temps | | | |
| Mémoire (espace de travail) | formule exacte de `workspace_size` en fonction de l, h et du nombre d'indices | | |

## 4. Mise en page de l'espace de travail

| Zone | Taille | Rôle |
|---|---|---|
| | | |

## 5. Limites et comportements

- Limites respectées (`max_nodes`, `max_time_ms`, `max_memory_bytes`) : <comment>
- Grille d'entrée partiellement remplie : <comment>
- Comptage de solutions : supporté / non supporté
- Résultats possibles et conditions de chacun : `SOLVED` / `UNSOLVABLE` / `INCOMPLETE` / `LIMIT_REACHED`

## 6. Plan de test

- [ ] Cas limites de [TESTING.md §4.2](../TESTING.md#42-cas-limites-obligatoires)
- [ ] Propriétés applicables (P-S1 à P-S6), avec la justification des exclusions
- [ ] Test différentiel contre : <solvers>
- [ ] Aucune allocation pendant `solve` (P-S6)
- [ ] Tests spécifiques à l'algorithme : <liste>

## 7. Mesures

| Corpus | Résolus | Temps médian | Pic mémoire | Commit et machine |
|---|---|---|---|---|
| tiny | | | | |
| small | | | | |

## 8. Passage à l'échelle et parallélisation

- Taille de grille maximale traitable sur une machine (selon la formule de mémoire du §3) :
- Parties parallélisables (lignes indépendantes, branches de recherche…) et dépendances :
- Obstacles à une version distribuée :

## 9. Pistes d'amélioration

<Ce qui n'est pas fait, et pourquoi.>
