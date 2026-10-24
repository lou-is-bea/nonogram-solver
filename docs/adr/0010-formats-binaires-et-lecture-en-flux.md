# ADR-0010 : Formats binaires et lecture en flux

- **Statut** : Accepté (2026-10-25)
- **Date** : 2026-10-25
- **Exigences liées** : F-03, F-07, F-08, NF-14
- **Complète** : [ADR-0006](0006-formats-de-fichiers.md), qui reste valable pour les grilles de taille courante

## Contexte

Les formats texte `.non` et `.sol` ([ADR-0006](0006-formats-de-fichiers.md)) ne passent pas à l'échelle des grilles massives ([ADR-0009](0009-types-et-grilles-massives.md)) :

- une grille 10⁵ × 10⁵ au format `.sol` pèse 10 Go (un octet par case, plus les fins de ligne), contre 2,5 Go à 2 bits par case ;
- charger un fichier entier en mémoire avant de l'analyser double l'empreinte mémoire ;
- un nœud de calcul doit pouvoir lire **sa portion** d'une grille sans lire tout le fichier, ce qui suppose des positions calculables.

## Options étudiées

- **Rester en texte, compressé (gzip, zstd)** : réduit la taille, mais ajoute une dépendance, et l'accès direct à une position devient impossible.
- **HDF5** : un standard du calcul haute performance, avec accès parallèle, mais une dépendance lourde et une API complexe, disproportionnées pour deux tableaux simples.
- **Formats binaires maison, simples, versionnés et petit-boutistes**, lus et écrits en flux.

## Décision

### 1. Lecture en flux pour tous les formats (dès M1)

- Une abstraction `ng_reader` (une fonction de lecture et un contexte), symétrique de `ng_writer`, s'applique à un fichier, à un tampon mémoire ou à un futur flux réseau.
- Les analyseurs lisent par **blocs de taille fixe** (64 Kio). La mémoire nécessaire à la lecture est en O(1), en plus de l'objet construit. Il n'y a **aucune limite** de taille de fichier ni de longueur de ligne.
- Les positions dans un fichier (ligne, colonne) sont de type `uint64_t`.

### 2. Formats binaires (à M9)

Les deux formats partagent un en-tête de 32 octets, petit-boutiste, lu octet par octet (sans `memcpy` d'une structure, pour ne dépendre ni de l'alignement ni de l'ordre des octets de la machine) :

| Décalage | Taille | Champ |
|---|---|---|
| 0 | 4 | signature : `NGP\0` (puzzle) ou `NGG\0` (grille) |
| 4 | 2 | version du format (1) |
| 6 | 2 | drapeaux (0 en version 1) |
| 8 | 4 | largeur (`ng_dim`) |
| 12 | 4 | hauteur (`ng_dim`) |
| 16 | 8 | taille de la charge utile, en octets |
| 24 | 4 | CRC-32C de la charge utile |
| 28 | 4 | réservé (0) |

- **`.ngp` (puzzle)** : pour chaque ligne, les rangées puis les colonnes, on écrit le nombre de blocs puis chaque longueur, toutes en entiers à longueur variable (LEB128 non signé). Un bloc de moins de 128 cases tient sur un octet.
- **`.ngg` (grille)** : 2 bits par case, ligne par ligne, 4 cases par octet en commençant par les bits de poids faible. `00` signifie inconnue, `01` vide et `10` pleine, `11` est invalide. La case d'index `i` se trouve à l'octet `32 + i / 4` : une portion de la grille se lit donc **directement à sa position**, ce qui permet les lectures parallèles (par exemple MPI-IO).
- Un fichier mal formé (signature, version, CRC, taille, valeur `11`) produit `NG_ERR_PARSE` avec un message précis. Les deux analyseurs sont soumis au fuzzing.

### 3. Outils

- `nonogram convert` passe d'un format à l'autre (`.non` ↔ `.ngp`, `.sol` ↔ `.ngg`).
- `nonogram generate --width --height --density --seed` génère en flux une grille aléatoire et ses indices, directement en binaire. La mémoire utilisée est en O(largeur + nombre de blocs des colonnes) : les grilles massives de benchmark peuvent donc être produites sans tenir en mémoire.
- Les grilles massives du corpus de benchmark ne sont **pas versionnées** : seuls leurs paramètres de génération (dimensions, densité, graine) et l'empreinte (CRC) attendue des fichiers le sont.

## Conséquences

- **Positives** : la taille est divisée par 4 ou plus, l'accès direct permet la parallélisation des entrées-sorties, l'empreinte mémoire de lecture est constante, les corpus massifs sont reproductibles sans stockage, et aucune dépendance n'est ajoutée.
- **Négatives et risques** : deux formats supplémentaires à maintenir et à tester. Les fichiers binaires ne sont pas lisibles dans un diff : les tests d'aller-retour (`convert` dans les deux sens) et les tests de propriétés compensent.
- **Actions** : `ng_reader` et des analyseurs en flux à M1. `.ngp`, `.ngg`, `convert` et `generate` à M9. [FILE_FORMATS.md](../FILE_FORMATS.md) sera complété à ce moment-là.

## Critères de réexamen

- Si les clusters cibles imposent un format standard (HDF5, ADIOS2).
- Si un encodage sur 1 bit pour les grilles complètes, ou un index d'accès direct pour `.ngp`, devient nécessaire. Ce sera alors une version 2 du format, signalée par les drapeaux.
