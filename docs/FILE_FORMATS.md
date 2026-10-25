# Formats de fichiers

> Dernière revue : 2026-10-25. Décisions : [ADR-0006](adr/0006-formats-de-fichiers.md) (formats texte), [ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md) (lecture en flux, formats binaires).

---

## 1. Puzzle : format `.non`

Le format est compatible avec le format `.non` de Steve Simpson, très répandu, ce qui permet de réutiliser des corpus existants. Le projet en lit un **sous-ensemble strict**, défini ci-dessous.

### 1.1 Exemple

```
# Exemple : un « P » minuscule
title "Lettre P"
author "nonogram-solver"
width 5
height 5

rows
3
1,1
3
1
1

columns
5
1,1
3
0
0
```

### 1.2 Grammaire

```
fichier     = { ligne_vide | commentaire | directive } , section_rows , section_cols ,
              { ligne_vide | commentaire | directive } ;
directive   = mot_clé , espaces , valeur , fin_ligne ;
mot_clé     = "title" | "author" | "copyright" | "license" | "catalogue"
            | "width" | "height" | "goal" ;
section_rows = "rows" , fin_ligne , ligne_indices × height ;
section_cols = "columns" , fin_ligne , ligne_indices × width ;
ligne_indices = ( "0" | entier , { sep , entier } ) , fin_ligne ;
sep         = "," | " " ;                             (* espaces autorisés autour des virgules *)
entier      = chiffre_non_nul , { chiffre } ;
commentaire = "#" , { caractère } , fin_ligne ;
fin_ligne   = "\n" | "\r\n" ;
```

### 1.3 Règles

- `width` et `height` sont **obligatoires** et doivent précéder les sections `rows` et `columns`.
- La section `rows` contient exactement `height` lignes d'indices, et la section `columns` exactement `width`.
- Les indices sont donnés de gauche à droite (pour les rangées) et de haut en bas (pour les colonnes).
- `0` désigne une ligne sans bloc. Une ligne de texte vide n'est **pas** acceptée à l'intérieur d'une section : on écrit `0`. Cela évite toute ambiguïté avec les lignes vides qui séparent les sections.
- Les commentaires (`#` en début de ligne) sont autorisés partout, y compris dans une section, et ne comptent pas comme une ligne d'indices.
- Les valeurs de `title` et `author` sont entre guillemets doubles, et ne doivent pas dépasser 255 octets.
- `goal` (une solution encodée, par exemple `"0110..."`) est accepté et conservé, mais il n'est pas utilisé par les solvers. Dans les tests, il peut servir de solution attendue.
- Toute directive inconnue provoque une **erreur** (`NG_ERR_PARSE`). Un format strict évite les surprises. Si l'import de corpus externes l'exige, un mode tolérant pourra être ajouté plus tard, par un ADR.
- Le fichier est encodé en UTF-8, sans BOM. Les fins de ligne `\n` et `\r\n` sont acceptées toutes les deux.
- Le fichier est lu **en flux**, par blocs de 64 Kio. Il n'y a aucune limite de taille de fichier ni de longueur de ligne : une ligne d'indices d'une grille massive peut contenir des millions de blocs.
- Les valeurs numériques sont accumulées avec `ckd_*` : un indice qui dépasse `NG_DIM_MAX` produit une erreur, jamais un débordement silencieux.
- Toutes les directives de texte (`title`, `author`, `copyright`, `license`, `catalogue`, `goal`) ont une valeur entre guillemets doubles, sans séquence d'échappement : la valeur s'arrête au premier `"`, et un caractère de contrôle autre que la tabulation y est refusé. La limite de 255 octets s'applique à toutes, sauf `goal` (dont la longueur suit le nombre de cases).
- Une directive répétée est une erreur (`duplicate directive 'title'`), de même qu'une section répétée.
- Entre les sections `rows` et `columns`, seuls les lignes vides et les commentaires sont acceptés ; les directives vont avant `rows` ou après les indices des colonnes.
- Une ligne qui ne contient que des espaces compte comme une ligne vide. Les séparateurs d'indices sont la virgule et l'espace uniquement : une tabulation est refusée.
- La dernière ligne peut ne pas se terminer par une fin de ligne.
- Un `\r` qui n'est pas suivi de `\n` est refusé.

### 1.4 Erreurs signalées

Chaque erreur précise sa ligne et sa colonne (sur 64 bits) et un message (`ng_parse_error`) :

| Cas | Message (indicatif) |
|---|---|
| `width` ou `height` manquant | `missing 'width' directive` |
| dimension égale à 0 ou supérieure à `NG_DIM_MAX` | `width out of range [1, 2147483647]: 0` |
| nombre de lignes d'indices incorrect | `expected 5 row clues, got 4` |
| indice non numérique, nul (sauf seul) ou trop grand | `invalid clue '1a' at position 2` |
| ligne d'indices trop longue pour la grille | `row 3 clues need 7 cells, width is 5` |
| totaux des rangées et des colonnes différents | `row clues sum to 12, column clues to 11` |
| directive inconnue (mode strict) | `unknown directive 'colour'` |

Les deux dernières vérifications (ligne trop longue, totaux différents) renvoient `NG_ERR_INCONSISTENT` ; les autres renvoient `NG_ERR_PARSE`. L'analyseur les fait lui-même pendant la lecture, pour indiquer la ligne du fichier concernée, avant que `ng_puzzle_create` ne les refasse. Les numéros de rangée et de colonne des messages commencent à 1.

Les métadonnées (`title`, `author`, …) ne font pas partie du puzzle : `ng_io_read_non` les renvoie dans un objet séparé, `ng_non_metadata`, et `ng_io_write_non` les accepte en paramètre.

## 2. Grille : format `.sol`

### 2.1 Exemple

```
###..
#.#..
###..
#....
#....
```

### 2.2 Règles

- Le fichier contient exactement `height` lignes de `width` caractères chacune.
- `#` désigne une case pleine, `.` une case vide et `?` une case inconnue. `?` est autorisé pour les grilles partielles (sortie de S2, entrée partiellement remplie).
- Aucun autre caractère n'est accepté, à l'exception des fins de ligne (`\n` ou `\r\n`, comme pour `.non`). La fin de ligne de la dernière rangée est facultative ; une ligne vide est refusée, y compris en fin de fichier. Il n'y a ni commentaire ni en-tête : les dimensions se déduisent du contenu, et le contrôle de cohérence avec le puzzle se fait au moment de la vérification.
- Un fichier écrit par `ng_io_write_sol` se termine toujours par `\n`.
- Les dimensions ne sont connues qu'en fin de lecture : les cases sont accumulées avant la création de la grille, soit environ deux fois la taille de la grille au pic.
- Ce format coûte un octet par case. Pour les grilles massives, on utilise le format binaire `.ngg` (§3).

## 3. Formats binaires `.ngp` et `.ngg` (M9)

Ces formats sont destinés aux grilles massives : 2 bits par case pour `.ngg`, indices en entiers à longueur variable pour `.ngp`, en-tête versionné de 32 octets avec un CRC-32C, et position de chaque case calculable directement. Leur spécification de référence est l'[ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md). Elle sera reprise ici en détail lors de leur implémentation.

## 4. Sorties de la CLI

- **Texte** (par défaut) : le rendu décrit dans [ROADMAP.md, M2](ROADMAP.md#m2--affichage).
- **JSON** (`--format=json`, pour `solve` et `bench`), avec un schéma versionné :

```json
{
  "schema": "nonogram-solve/1",
  "puzzle": "tests/data/puzzles/small/duck.non",
  "solver": "dpsearch",
  "result": "SOLVED",
  "verified": true,
  "stats": { "nodes": 42, "line_solves": 318, "elapsed_ns": 120400, "workspace_bytes": 18432, "solutions_found": 1 },
  "grid": ["###..", "#.#..", "###..", "#....", "#...."]
}
```

Toute modification incompatible du JSON incrémente le numéro de `schema`.
