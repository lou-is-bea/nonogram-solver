# ADR-0006 : Formats `.non` (entrée) et `.sol` (grille)

- **Statut** : Accepté
- **Date** : 2026-10-24
- **Exigences liées** : F-03, F-06

## Contexte

Il faut un format de puzzle facile à écrire à la main, facile à analyser de façon sûre en C, et compatible avec des corpus existants pour les benchmarks. Il faut aussi un format de grille simple à comparer dans les tests.

## Options étudiées

- **Format maison** : contrôle total, mais incompatible avec les corpus existants.
- **`.non` (Steve Simpson)** : format texte répandu, que reconnaissent plusieurs outils et collections.
- **JSON** : universel, mais demande un analyseur JSON en C (une dépendance ou beaucoup de code), et il est verbeux à la main.
- **XML (webpbn)** : riche (couleurs), mais lourd à analyser.

## Décision

- En entrée, on lit un **sous-ensemble strict de `.non`**, défini dans [FILE_FORMATS.md](../FILE_FORMATS.md). Toute directive inconnue est une erreur.
- Pour les grilles, on utilise un format texte **`.sol`** (`#`, `.` et `?`), lisible dans un diff.
- Les sorties machine de la CLI sont en JSON avec un schéma versionné. Le JSON est seulement **écrit**, jamais lu, ce qui ne demande pas d'analyseur.

## Conséquences

- **Positives** : les corpus existants sont réutilisables, l'analyseur est simple et peut être soumis au fuzzing, et les tests sont lisibles.
- **Négatives** : pas de couleurs pour l'instant. Certains fichiers `.non` du commerce utilisent des directives hors du sous-ensemble et seront refusés.
- **Actions** : `src/io/`, `fuzz_non_parser`.

## Critères de réexamen

Le support des nonogrammes en couleur, ou l'import d'un corpus au format XML webpbn.
