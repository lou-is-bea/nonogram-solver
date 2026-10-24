# ADR-0009 : Types dédiés et support des grilles massives

- **Statut** : Accepté (2026-10-25)
- **Date** : 2026-10-25
- **Exigences liées** : F-01, NF-05, NF-14, NF-15

## Contexte

Certains solvers seront ensuite **massivement parallélisés**, pour tourner sur des architectures de plusieurs centaines de nœuds de calcul. Mesurer leurs performances demande des grilles **massives**, bien au-delà de la limite de 2048 × 2048 et des types `uint16_t` prévus au départ.

Ordres de grandeur de la mémoire nécessaire pour la seule grille :

| Dimensions | Cases | 1 octet par case | 2 bits par case |
|---|---|---|---|
| 10⁴ × 10⁴ | 10⁸ | 100 Mo | 25 Mo |
| 10⁵ × 10⁵ | 10¹⁰ | 10 Go | 2,5 Go |
| 10⁶ × 10⁶ | 10¹² | 1 To | 250 Go |

Deux conséquences :

1. Un **index linéaire de case** (`ligne × largeur + colonne`) dépasse 2³² dès 65 536 × 65 536. Il doit donc être sur 64 bits.
2. Au-delà d'environ 10⁵ × 10⁵, une grille ne tient plus dans un seul nœud de calcul. Le modèle de données ne doit pas supposer, à long terme, que la grille entière est contiguë dans un seul espace d'adressage.

Les **indices** peuvent aussi peser lourd. Une grille aléatoire de densité 0,5 compte environ n/4 blocs par ligne, soit environ n²/2 blocs au total. Stockés sur 4 octets, cela représente environ 2 octets par case, plus que la grille elle-même. Les puzzles « réels » ont beaucoup moins de blocs, mais les benchmarks massifs utiliseront des grilles générées.

## Options étudiées

- **A. Garder des types bruts et les élargir** (`uint32_t` et `uint64_t` écrits partout) : simple, mais le sens de chaque entier se perd (une dimension, un index de case et un compteur se ressemblent), et tout futur changement de largeur demande de modifier toute la base de code.
- **B. `size_t` partout** : supprime les débordements, mais double la mémoire des indices et se confond avec les tailles d'allocation.
- **C. Des types sémantiques propres au projet**, définis à un seul endroit, avec des bornes et des invariants vérifiés à la compilation, et un stockage opaque derrière une API d'accès par ligne.

## Décision

Option **C**.

### 1. Types définis dans `include/nonogram/types.h`

| Type | Définition | Rôle | Borne |
|---|---|---|---|
| `ng_dim` | `uint32_t` | largeur, hauteur, longueur d'une ligne, position dans une ligne, longueur d'un bloc | `NG_DIM_MAX` = 2³¹ − 1 |
| `ng_line_id` | `uint32_t` | identifiant de ligne unifié (`0 .. h+w−1`) | `h + w ≤ 2³² − 2`, garanti par `NG_DIM_MAX` |
| `ng_run_count` | `uint32_t` | nombre de blocs d'une ligne | `≤ (NG_DIM_MAX + 1) / 2` |
| `ng_cell_index` | `uint64_t` | index linéaire d'une case, nombre de cases | `≤ NG_DIM_MAX²` (< 2⁶²) |
| `ng_run_index` | `uint64_t` | position dans le stockage global des blocs, nombre total de blocs | `≤` nombre de cases + nombre de lignes |
| `ng_counter` | `uint64_t` | compteurs de statistiques (nœuds de recherche, résolutions de lignes) | — |

Les tailles mémoire restent en `size_t`. Les bornes sont des `constexpr` (C23). Les invariants sont vérifiés par `static_assert` :

```c
constexpr ng_dim NG_DIM_MAX = (ng_dim)INT32_MAX;
static_assert((uint64_t)NG_DIM_MAX * 2u <= UINT32_MAX, "line ids must fit in ng_line_id");
static_assert((uint64_t)NG_DIM_MAX * NG_DIM_MAX <= UINT64_MAX / 4u, "cell indices must fit in ng_cell_index");
```

Des macros de format (`NG_PRI_DIM`, `NG_PRI_CELL_INDEX`…) servent à l'affichage.

### 2. Règles d'arithmétique

- Le produit de deux `ng_dim` se calcule **toujours** après conversion en `ng_cell_index`, via `ngi_cell_index(row, col, width)`. L'oubli de cette conversion est détecté par clang-tidy (`bugprone-implicit-widening-of-multiplication-result`).
- Tout calcul de taille d'allocation passe par `ckd_add` et `ckd_mul` (`<stdckdint.h>`, voir l'[ADR-0001](0001-langage-c23.md)). La grille est refusée avec `NG_ERR_LIMIT` si sa taille n'est pas représentable, et avec `NG_ERR_OUT_OF_MEMORY` si l'allocateur échoue.
- Dans un même processus, la seule limite pratique est la mémoire. Les limites `NG_DIM_MAX` ne sont que des bornes de représentation.

### 3. Stockage opaque et accès par ligne

- La représentation des cases (un octet par case au départ, puis 2 bits par case) et des indices (un tableau CSR au départ, puis éventuellement un encodage compact) est un **détail interne de `core`**.
- L'API publique et les solvers n'accèdent jamais à un pointeur sur le stockage. Ils travaillent sur des **copies de ligne** :
  - `ng_grid_load_line(grid, line, ng_cell *buffer)` et `ng_grid_store_line(grid, line, const ng_cell *buffer)` ;
  - `ng_puzzle_line_run_count(puzzle, line)` et `ng_puzzle_copy_line_runs(puzzle, line, ng_dim *buffer, capacity)`.
- La copie coûte O(n) par ligne, ce qui est négligeable devant une résolution de ligne en O(n·k). En contrepartie, la ligne devient l'**unité de travail et d'échange** naturelle pour la parallélisation, et le stockage peut changer (compactage, partitionnement entre nœuds) sans modifier les solvers.

### 4. Profil compact (non retenu pour l'instant)

Un profil de compilation à 16 bits (`ng_dim = uint16_t`, `ng_cell_index = uint32_t`) pourrait réduire la mémoire et améliorer l'usage du cache sur les petites grilles. Il n'est **pas** implémenté. Les types sémantiques le rendent possible plus tard, par un nouvel ADR, si les benchmarks montrent un gain significatif.

## Conséquences

- **Positives** : les grilles massives sont représentables dès M1, sans refonte ultérieure. Le code est plus lisible (le type porte le sens) et la largeur des types se change à un seul endroit. Le stockage est découplé des algorithmes, ce qui prépare la compression et la distribution.
- **Négatives et risques** :
  - les indices sur 32 bits doublent la mémoire par rapport à 16 bits sur les petites grilles. C'est acceptable : la grille et l'espace de travail des solvers dominent ;
  - un calcul fait en 32 bits peut déborder silencieusement. Les règles d'arithmétique, clang-tidy, UBSan (`-fsanitize=integer` sous Clang) et des tests dédiés le préviennent ;
  - certains tests de limites ne peuvent pas allouer réellement : ils utilisent un allocateur plafonné ([TESTING.md §4.2](../TESTING.md#42-cas-limites-obligatoires)).
- **Actions** : `types.h` (M1), mise à jour de [ARCHITECTURE.md](../ARCHITECTURE.md) et des normes, label de test `massive` exécuté en nocturne, formats binaires ([ADR-0010](0010-formats-binaires-et-lecture-en-flux.md)), étape M9 de la [feuille de route](../ROADMAP.md).

## Critères de réexamen

- Les benchmarks montrent qu'un profil compact apporte plus de 15 % sur les grilles petites et moyennes.
- Le besoin de dépasser 2³¹ − 1 cases de côté apparaît (aucun cas d'usage connu).
