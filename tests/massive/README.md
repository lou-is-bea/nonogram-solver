# Tests sur grilles massives

Stratégie : [TESTING.md §4.6](../../docs/TESTING.md#46-grilles-massives). Ces tests portent le label CTest `massive` et sont exclus des presets de tous les jours (`dev`, `dev-clang`, `release`, `asan`, `ubsan`, `coverage`). Ils sont lancés en nocturne (porte G12) et à la demande :

```sh
cmake --preset release && cmake --build --preset release
ctest --preset massive
```

## Paliers et dimensionnement

| Palier | Contenu | Machine | Durée mesurée |
|---|---|---|---|
| 10⁴ × 10⁴ (10⁸ cases) | `test_massive.c` : grille complète (formule mémoire à ± 5 %, chaque rangée et colonne relue), puzzle d'environ 2 × 10⁶ blocs (formule mémoire à ± 5 %), allers-retours `.non` et `.sol` ; limite 10⁵ × 10⁵ vérifiée avec l'allocateur plafonné | poste de développement (aller-retour `.sol` : texte, tampon de lecture et grille) | 1,5 s en Release (16 cœurs, 15 Go, WSL2), environ 300 Mo de RAM résidente au pic |
| 10⁵ × 10⁵ (10¹⁰ cases) | à venir (M9.1, stockage à 2 bits par case et formats binaires) | machine dédiée, au moins 64 Go (question Q7) | — |

Les grilles sont **générées** (motif déterministe de bandes diagonales), jamais stockées dans le dépôt.
