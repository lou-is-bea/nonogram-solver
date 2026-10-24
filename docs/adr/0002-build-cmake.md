# ADR-0002 : CMake, CTest et presets comme système de build

- **Statut** : Accepté
- **Date** : 2026-10-24 (révisé le 2026-10-25 : environnement Linux uniquement ; la décision elle-même est inchangée)
- **Exigences liées** : NF-02, NF-11

## Contexte

Le build doit fonctionner sous Linux (Ubuntu sous WSL2 pour le développement, Linux pour la CI et les clusters de calcul), proposer plusieurs configurations (Debug, Release, sanitizers, couverture, fuzzing) et **imposer les dépendances entre couches**.

## Options étudiées

- **Make seul** : simple, mais les configurations multiples, la détection des dépendances et l'intégration aux IDE doivent être gérées à la main.
- **Meson** : élégant et rapide, mais moins répandu dans les IDE et nécessite Python.
- **CMake (≥ 3.21)** : standard de fait, supporté nativement par CLion et VS Code (y compris en mode distant WSL). CTest est intégré, et `CMakePresets.json` donne des configurations reproductibles.

## Décision

Le projet utilise **CMake ≥ 3.21**, avec une cible par couche (`ng_core`, `ng_io`, etc.) dont les dépendances sont déclarées par `target_link_libraries`, **CTest** comme lanceur de tests (avec les labels `unit`, `integration` et `property`), et **CMakePresets.json** pour les configurations `dev`, `release`, `asan`, `ubsan`, `coverage` et `fuzz`, avec le générateur Ninja.

## Conséquences

- **Positives** : les mêmes commandes servent en local et en CI. Une dépendance interdite entre couches provoque une erreur d'édition de liens.
- **Négatives** : la syntaxe CMake est verbeuse. Pour la limiter, on n'utilise que le CMake « moderne », orienté cibles, sans variables globales comme `CMAKE_C_FLAGS` dans les sous-dossiers.
- **Actions** : M0.

## Critères de réexamen

Si la maintenance du CMake devient un frein notable.
