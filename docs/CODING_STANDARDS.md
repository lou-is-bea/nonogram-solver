# Normes de code

> Dernière revue : 2026-10-25. Ce document est **normatif**. Les règles marquées 🔧 sont vérifiées automatiquement (compilateur, clang-format, clang-tidy ou cppcheck). Les autres sont vérifiées en revue.

---

## 1. Langage et compilation

- **C23 strict** (`-std=c23`), sans extension GNU ([ADR-0001](adr/0001-langage-c23.md)). Compilateurs supportés : **GCC ≥ 14** et **Clang ≥ 19**, sous Linux. 🔧
- Les mêmes options de warnings s'appliquent à tout le code du projet (définies une fois dans le CMake racine) : 🔧

  ```
  -std=c23 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wsign-conversion -Wshadow
  -Wstrict-prototypes -Wmissing-prototypes -Wvla -Wcast-qual -Wformat=2
  -Wnull-dereference -Wdouble-promotion -Wundef -Wimplicit-fallthrough
  ```

- Les **VLA sont interdits** (`-Wvla`). 🔧
- Criterion est inclus comme en-tête SYSTEM : nos options de warnings ne s'appliquent pas à ses en-têtes, mais elles s'appliquent bien au code de nos tests.

### Fonctionnalités C23

| Fonctionnalité | Usage |
|---|---|
| `nullptr` | **obligatoire** à la place de `NULL` 🔧 |
| `bool`, `true`, `false` (mots-clés) | obligatoires ; ne pas inclure `<stdbool.h>` |
| `static_assert` | encouragé pour les invariants de types et de mise en page |
| `[[nodiscard]]` | obligatoire sur toute fonction qui renvoie un `ng_status` 🔧 |
| `[[maybe_unused]]`, `[[fallthrough]]` | à la place des conversions en `(void)` et des commentaires d'intention |
| `constexpr` | à préférer à `#define` pour les constantes typées |
| Énumérations à type sous-jacent fixe | obligatoires pour les énumérations stockées (`enum ng_cell : uint8_t`) |
| `<stdckdint.h>` (`ckd_add`, `ckd_mul`, `ckd_sub`) | **obligatoire** pour tout calcul de taille et tout calcul sur des index susceptible de déborder |
| `<stdbit.h>` | pour les opérations sur les bits (comptage, recherche de bit) |
| Séparateurs de chiffres (`1'000'000`), littéraux binaires (`0b1010`) | autorisés pour la lisibilité |
| `typeof` | uniquement dans les macros génériques de `src/core/internal/` |
| `unreachable()` | uniquement via `NG_UNREACHABLE()`, qui commence par une assertion en Debug |
| `auto` (inférence de type), `_BitInt`, `#embed` | **interdits** sauf ADR justifiant leur usage |
| Attributs de fabricant dans la syntaxe standard (`[[gnu::format(printf, n, m)]]`) | autorisés **uniquement** s'ils n'ajoutent que des diagnostics (le code reste du C23 strict si le compilateur les ignore) ; décision du 2026-10-25 |

## 2. Nommage

| Élément | Convention | Exemple |
|---|---|---|
| Fonction ou type public | `ng_<module>_<action>` | `ng_grid_create`, `ng_puzzle` |
| Fonction ou type interne partagé entre fichiers | `ngi_<module>_<action>` | `ngi_cell_index`, `ngi_line_queue` |
| Fonction propre à un fichier | `static`, sans préfixe | `static bool parse_run_list(...)` |
| Macro, constante, constante d'énumération | `NG_UPPER_SNAKE` | `NG_DIM_MAX`, `NG_CELL_FILLED` |
| Variable, paramètre | `snake_case`, descriptif | `run_count`, `line_id` |
| Fichier | `snake_case.c` / `.h` | `non_parser.c` |

- Aucun type ne prend le suffixe `_t`, réservé par POSIX.
- Les noms d'une lettre sont réservés aux indices de boucles courtes (`i`, `j`) et aux notations mathématiques documentées (`n`, `k`).
- Un booléen se nomme par une affirmation : `is_complete`, `has_contradiction`.

## 3. Fichiers et en-têtes

- Un module logique correspond à une paire `.h` / `.c`. Un fichier ne doit pas dépasser environ 500 lignes ; au-delà, on le découpe.
- Chaque en-tête a une garde d'inclusion de la forme `NG_<CHEMIN>_H` (par exemple `NG_NONOGRAM_GRID_H`). On n'utilise pas `#pragma once`, qui ne fait pas partie du standard.
- Chaque en-tête est **autonome** : il compile seul. Un test de compilation par en-tête public le vérifie. 🔧
- Ordre des `#include` : (1) l'en-tête correspondant au fichier, (2) la bibliothèque standard, (3) l'API publique du projet, (4) les en-têtes internes. Chaque groupe est séparé par une ligne vide et trié. 🔧
- Les en-têtes publics n'incluent que la bibliothèque standard et d'autres en-têtes publics.

## 4. Types et arithmétique

Les types sont fixés par l'[ADR-0009](adr/0009-types-et-grilles-massives.md).

- On utilise **les types sémantiques** de `types.h` : `ng_dim` pour les dimensions, longueurs et positions dans une ligne, `ng_line_id`, `ng_run_count`, `ng_cell_index` pour les index de case, `ng_run_index` et `ng_counter`.
- `size_t` est réservé aux tailles mémoire. `int` n'est jamais utilisé pour une taille, un index ou une dimension.
- **Le produit de deux `ng_dim` se calcule en `ng_cell_index`** : on utilise `ngi_cell_index(row, col, width)`, ou on convertit explicitement un opérande avant la multiplication. 🔧 (clang-tidy `bugprone-implicit-widening-of-multiplication-result`)
- Tout calcul de taille d'allocation, et toute somme ou tout produit qui peut déborder, passe par `ckd_add` ou `ckd_mul`. Le résultat est testé. 🔧 (revue et recherche dans `check.sh`)
- Les conversions entre types signés et non signés ou entre largeurs différentes sont **explicites** et justifiées si elles peuvent perdre de l'information. 🔧
- Pour afficher ces types, on utilise les macros de format `NG_PRI_DIM`, `NG_PRI_CELL_INDEX`, etc.
- Le résultat d'une comparaison n'est jamais utilisé comme un entier (vérifié en revue : `readability-implicit-bool-conversion` signale en C toute expression booléenne, une comparaison y étant de type `int`).

## 5. Fonctions

- **Une responsabilité par fonction.** On vise moins de 50 lignes par fonction. La complexité cognitive est limitée à 25 (clang-tidy `readability-function-cognitive-complexity`). 🔧 Exception : le code de test (`tests/.clang-tidy`), où chaque assertion Criterion se développe en plusieurs branches (accord du responsable du projet, 2026-10-25).
- Une fonction prend au plus 6 paramètres. Au-delà, on les regroupe dans une structure d'options.
- La qualité `const` est appliquée partout où elle est possible (paramètres pointeurs en lecture seule, variables locales non modifiées).
- La documentation de chaque paramètre pointeur indique s'il peut valoir `nullptr`. Par défaut, il ne le peut pas.
- `restrict` et `inline` ne sont utilisés que s'ils améliorent une performance **mesurée**.
- **Pas de récursion** dans les solvers. Ailleurs, la récursion n'est acceptée que si sa profondeur est bornée par une petite constante et que cette borne est documentée.

## 6. Gestion des erreurs

- Toute fonction publique qui peut échouer renvoie un `ng_status` marqué `[[nodiscard]]`. Les résultats passent par des pointeurs de sortie placés en dernier. 🔧
- La bibliothèque n'utilise jamais `exit`, `abort` ni `printf` vers `stdout` ou `stderr`.
- `NG_ASSERT` sert aux invariants internes, jamais à la validation des entrées. Toute entrée provenant de l'appelant est validée et une erreur est renvoyée. Un accesseur qui ne peut pas renvoyer de statut (`ng_grid_get`, `ng_puzzle_line_length`…) documente une valeur de repli pour les arguments invalides et l'applique dans tous les builds.
- Le nettoyage se fait avec un **unique** `goto cleanup;` en fin de fonction. C'est le seul usage autorisé de `goto`.

```c
[[nodiscard]] ng_status ng_grid_create(const ng_allocator *a, ng_dim width, ng_dim height,
                                       ng_grid **out)
{
    if (a == nullptr || out == nullptr) {
        return NG_ERR_INVALID_ARGUMENT;
    }
    *out = nullptr;
    if (width == 0 || height == 0 || width > NG_DIM_MAX || height > NG_DIM_MAX) {
        return NG_ERR_LIMIT;
    }
    ng_cell_index cell_count = 0;
    if (ckd_mul(&cell_count, (ng_cell_index)width, (ng_cell_index)height)) {
        return NG_ERR_LIMIT;
    }
    size_t bytes = 0;
    if (ckd_add(&bytes, sizeof(struct ng_grid), cell_count)) {
        return NG_ERR_LIMIT;
    }
    /* ... une allocation via a->alloc, initialisation ... */
    return NG_OK;
}
```

## 7. Mémoire

Les règles détaillées sont dans l'[ADR-0004](adr/0004-gestion-memoire.md).

1. Toute allocation passe par un `const ng_allocator *`. Les appels directs à `malloc`, `calloc`, `realloc` et `free` ne sont autorisés que dans `allocator.c`. 🔧 (une règle de recherche dans `check.sh`)
2. **La propriété mémoire est explicite** : chaque fonction qui reçoit ou renvoie un pointeur indique dans sa documentation `@ownership` si l'appelant possède le pointeur, l'emprunte ou le transfère.
3. Chaque fonction `*_create` a sa fonction `*_destroy`. `*_destroy(nullptr)` ne fait rien.
4. Un objet correspond à une seule allocation contiguë, quand c'est possible.
5. **Aucune allocation dans une boucle chaude**, et en particulier jamais dans `solve()`.
6. Les structures sont initialisées à zéro (`= {}`, la forme C23) avant usage.
7. Après une libération, le pointeur local est remis à `nullptr` s'il reste dans la portée.
8. Aucun code n'accède directement au stockage des cases ou des indices hors de `core`. On passe par l'API d'accès par ligne ou par case ([ARCHITECTURE.md §4.4](ARCHITECTURE.md#44-grille-ng_grid-opaque)).

## 8. Fonctions et constructions interdites

| Interdit | Utiliser à la place | Raison |
|---|---|---|
| `NULL` | `nullptr` | typage (C23) |
| `strcpy`, `strcat`, `sprintf`, `gets` | `memcpy` avec une taille connue, `snprintf` | débordements |
| `atoi`, `atol`, `sscanf` pour lire des nombres | l'analyseur dédié, avec `ckd_*` pour l'accumulation | erreurs silencieuses |
| `strtok` | analyse manuelle | état global |
| `rand`, `srand` | le générateur avec graine de `tests/support` | non reproductible, état global |
| VLA, `alloca` | espace de travail pré-alloué | pile non bornée |
| Variables globales modifiables | contexte passé explicitement | NF-08 |
| Nombres magiques (hors données littérales des tests, voir `tests/.clang-tidy`) | `constexpr` ou constante d'énumération nommée | lisibilité |
| Lecture d'un fichier entier en mémoire | `ng_reader`, lecture par blocs | grilles massives ([ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md)) |
| `memcpy` d'une structure vers un format binaire | sérialisation octet par octet, petit-boutiste | portabilité du format |

🔧 Ces règles sont contrôlées par clang-tidy (`bugprone-*`, `cert-*`, `modernize-use-nullptr`) et par une recherche par motif dans `check.sh`.

## 9. Style et formatage

- **clang-format** fait foi. 🔧 La configuration de référence est la suivante :

  ```yaml
  BasedOnStyle: LLVM
  IndentWidth: 4
  ColumnLimit: 100
  BreakBeforeBraces: Linux
  AllowShortIfStatementsOnASingleLine: Never
  AllowShortFunctionsOnASingleLine: None
  PointerAlignment: Right
  SortIncludes: CaseSensitive
  IncludeBlocks: Regroup
  ```

- Les **accolades sont obligatoires**, même autour d'une instruction unique. 🔧 (clang-tidy `readability-braces-around-statements`)
- Une déclaration par ligne. Les variables sont déclarées au plus près de leur première utilisation.

## 10. Commentaires et documentation

- Chaque élément de l'API publique a un commentaire **Doxygen** : `@brief`, `@param`, `@return` (avec les statuts possibles), `@ownership` et, pour les solvers, `@complexity` (en temps et en mémoire, en fonction de l, h et du nombre de blocs).
- Les commentaires dans le code expliquent le **pourquoi** (invariant, choix d'algorithme, référence), pas le *quoi*.
- Le code et les commentaires sont rédigés en anglais ([ADR-0008](adr/0008-langues.md)).
- Les marqueurs `TODO` et `FIXME` sont suivis d'un numéro de ticket : `/* TODO(#42): ... */`. Ceux qui n'en ont pas sont refusés en revue.

```c
/**
 * @brief Verify that a complete grid satisfies every row and column clue.
 *
 * @param puzzle  Puzzle definition. Borrowed, must not be nullptr.
 * @param grid    Grid to check. Borrowed, must not be nullptr, same dimensions as @p puzzle.
 * @param report  Output report. Must not be nullptr. Fully written on NG_OK.
 * @return NG_OK on success (the verdict is in @p report),
 *         NG_ERR_INVALID_ARGUMENT on nullptr arguments or mismatched dimensions.
 * @complexity O(width * height) time, O(1) extra memory.
 */
[[nodiscard]] ng_status ng_verify(const ng_puzzle *puzzle, const ng_grid *grid,
                                  ng_verify_report *report);
```

## 11. Performance

1. **D'abord juste et lisible, ensuite rapide.** Une optimisation n'est acceptée que si un benchmark montre un gain.
2. Une version optimisée d'un algorithme garde une version de référence simple, utilisée dans les tests différentiels.
3. Les boucles chaudes sont signalées par un commentaire `/* HOT PATH: ... */`, qui rappelle qu'elles ne doivent pas allouer.
4. Tout algorithme dont le coût dépasse O(cases) en mémoire est signalé dans sa spécification, avec la taille de grille maximale qu'il peut traiter.
