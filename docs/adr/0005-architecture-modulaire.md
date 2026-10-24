# ADR-0005 : Bibliothèque en couches et solvers enfichables

- **Statut** : Accepté
- **Date** : 2026-10-24
- **Exigences liées** : F-30, NF-11

## Contexte

Le projet est itératif : plusieurs solvers seront ajoutés successivement, du plus simple au plus complexe, et devront être comparés. Ajouter un solver ne doit toucher ni au cœur ni aux autres solvers.

## Options étudiées

- **Un `switch` sur un type de solver** : simple, mais chaque ajout modifie un fichier central et le couplage augmente.
- **Chargement dynamique de greffons (`dlopen`, `LoadLibrary`)** : flexible, mais complexe, peu portable et sans intérêt pour un projet monolithique.
- **Table de fonctions et registre statique** : chaque solver exporte une `const ng_solver_vtable`, et `registry.c` contient un tableau de pointeurs vers ces tables.

## Décision

- La bibliothèque est organisée en **couches** (core, puis io, render, verify et solvers, puis cli), chacune étant une cible CMake distincte ([ARCHITECTURE.md §3](../ARCHITECTURE.md#3-modules-et-dépendances)).
- Les solvers implémentent `ng_solver_vtable` et sont référencés dans un **registre statique** en lecture seule. Ils sont sélectionnés par leur nom.
- Les types publics sont **opaques**. Les couches internes accèdent aux structures par des en-têtes `ngi_` non installés.

## Conséquences

- **Positives** : ajouter un solver revient à créer un dossier et à ajouter une ligne au registre. Tous les solvers sont comparables avec le même harnais. L'API publique reste stable.
- **Négatives** : un appel indirect par `solve()`, au coût négligeable. Les en-têtes internes créent un couplage entre couches, à contenir en revue.
- **Actions** : `solver.h`, `registry.c`, procédure P1 des [playbooks](../agents/PLAYBOOKS.md).

## Critères de réexamen

Si des solvers externes au dépôt doivent pouvoir être chargés.
