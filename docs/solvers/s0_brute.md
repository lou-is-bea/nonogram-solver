# Solver S0 : force brute

- **Nom dans le registre** : `brute`
- **Étape** : M4
- **Statut** : Spécifié
- **Complet** : oui (voir [GLOSSARY.md](../GLOSSARY.md))

## 1. Principe

S0 essaie toutes les grilles possibles, une par une, et garde celles qui respectent tous les indices. Il est volontairement naïf : son rôle est d'être un **oracle de référence** sur les petites grilles, simple au point d'être juste par construction, contre lequel les solvers suivants sont comparés (propriété P-S3). Il compte aussi les solutions, ce qui permet de vérifier l'unicité d'un puzzle.

Il ne dépend pas du vérificateur ([ADR-0007](../adr/0007-verificateur-oracle.md)) : il contrôle chaque grille candidate avec son propre code, et les tests passent ensuite chaque solution par `ng_verify`. Deux implémentations indépendantes doivent donc se tromper de la même façon pour qu'une erreur passe inaperçue.

## 2. Algorithme

Seules les cases **inconnues** de la grille d'entrée sont énumérées ; les cases déjà connues sont recopiées telles quelles (contrat §6.3 d'[ARCHITECTURE.md](../ARCHITECTURE.md#6-interface-des-solvers)). Soit `k` le nombre de cases inconnues.

**Ordre d'énumération.** Une grille candidate se lit comme un nombre binaire de `k` chiffres : la première case inconnue, dans l'ordre des lignes (de haut en bas, puis de gauche à droite), est le chiffre de poids fort ; `EMPTY` vaut 0 et `FILLED` vaut 1. S0 parcourt les candidates par ordre croissant, de « tout `EMPTY` » à « tout `FILLED` ». La première solution trouvée est donc la plus petite dans cet ordre, ce qui rend le résultat prévisible et testable.

```
init(ws, puzzle, options):
    copier les indices de toutes les lignes dans ws (offsets + longueurs des blocs)

solve(ws, grille, stats, résultat):
    charger la grille d'entrée ligne par ligne dans candidate[]   # ng_grid_load_line
    inconnues[] ← positions des cases UNKNOWN, dans l'ordre des lignes ; k ← leur nombre
    mettre toutes les cases inconnues de candidate[] à EMPTY
    trouvées ← 0
    boucle :
        si max_nodes ≠ 0 et nœuds = max_nodes : arrêt LIMIT_REACHED
        si nœuds est multiple de NGI_BRUTE_TIME_CHECK et le temps est dépassé : arrêt LIMIT_REACHED
        nœuds ← nœuds + 1
        si toutes les lignes de candidate[] respectent leur indice :          # lignes, puis colonnes
            si trouvées = 0 : première[] ← candidate[]
            trouvées ← trouvées + 1
            si trouvées = max_solutions : arrêt SOLVED
        # candidate suivante : +1 en binaire, en partant de la dernière case inconnue
        i ← k
        tant que i > 0 et candidate[inconnues[i − 1]] = FILLED :
            candidate[inconnues[i − 1]] ← EMPTY ; i ← i − 1
        si i = 0 : fin de l'énumération                                      # retenue sortie
        candidate[inconnues[i − 1]] ← FILLED
    fin de l'énumération : SOLVED si trouvées > 0, sinon UNSOLVABLE
    si une solution a été trouvée : écrire première[] dans la grille, rangée par rangée   # ng_grid_store_line
```

**Contrôle d'une ligne.** La ligne est parcourue une seule fois : chaque fois qu'un bloc de cases `FILLED` se termine, sa longueur est comparée au bloc attendu suivant de l'indice. La ligne est rejetée dès le premier écart, ou si le nombre de blocs diffère. Une ligne d'indice vide n'accepte aucune case `FILLED`. Le contrôle s'arrête à la première ligne fausse : les rangées sont contrôlées avant les colonnes.

**Passage à la candidate suivante.** C'est l'incrément binaire classique : on remet à `EMPTY` les cases `FILLED` de poids faible, puis on passe la suivante à `FILLED`. Le coût amorti est constant ; aucun compteur de `k` bits n'est nécessaire, donc la limite de 64 bits d'un entier ne s'applique pas à l'énumération elle-même.

**Nœud.** Pour S0, un nœud est **une grille candidate examinée**. Une énumération complète compte exactement `2^k` nœuds. `line_solves` reste à 0 : S0 n'a pas de solveur de ligne.

## 3. Complexité

Notations : `l` largeur, `h` hauteur, `N = l × h` cases, `k ≤ N` cases inconnues, `L = l + h` lignes, `R` nombre total de blocs (somme des tailles des indices).

| Ressource | Pire cas | Cas typique | Justification |
|---|---|---|---|
| Temps | `O(2^k × N)` | `O(2^k × l)` | `2^k` candidates ; le contrôle complet lit chaque case deux fois (rangées puis colonnes), mais la plupart des candidates sont rejetées dès la première rangée fausse. Avec `N ≤ 25`, le pire cas est d'environ `3,4 × 10⁷` candidates. |
| Mémoire (espace de travail) | `S + 10 N + 8 L + 4 R + 8` octets | moins de 1 Kio | formule exacte au §4 ; `N ≤ 25`, `L ≤ 26` |

## 4. Mise en page de l'espace de travail

Les zones sont rangées par alignement décroissant, ce qui évite tout remplissage entre elles.

| Zone | Taille (octets) | Rôle |
|---|---|---|
| état | `S = sizeof(ngi_brute_state)` | dimensions, `k`, options utiles, compteurs, pointeurs vers les zones suivantes |
| `inconnues` | `N × sizeof(ng_cell_index)` = `8 N` | positions des cases inconnues (au plus `N`) |
| `offsets` | `(L + 1) × sizeof(ng_run_index)` = `8 (L + 1)` | début des blocs de chaque ligne dans `blocs` |
| `blocs` | `R × sizeof(ng_dim)` = `4 R` | longueurs des blocs, ligne après ligne (copie des indices faite par `init`) |
| `candidate` | `N × sizeof(ng_cell)` = `N` | grille en cours d'examen, rangée par rangée |
| `première` | `N × sizeof(ng_cell)` = `N` | première solution trouvée, gardée pendant le comptage |

**Formule de `workspace_size`** : `S + 8 N + 8 (L + 1) + 4 R + 2 N = S + 10 N + 8 L + 4 R + 8`, calculée avec `ckd_add` et `ckd_mul`. Un test vérifie la partie variable de la formule par différences entre puzzles de tailles et d'indices différents (`10 ΔN + 8 ΔL + 4 ΔR`), et que `S` est le même pour tous ; il n'a donc pas besoin de connaître `S`.

L'espace de travail doit être aligné au moins sur `alignof(ng_cell_index)` ; `ng_solve` le fournit aligné sur `alignof(max_align_t)` (#10).

`workspace_size` renvoie `NG_ERR_LIMIT` dans trois cas :

- `N` dépasse `NG_BRUTE_MAX_CELLS`. Le produit `l × h` est calculé en `ng_cell_index` avec `ckd_mul`, donc un débordement est refusé de la même façon ;
- la taille n'est pas représentable ;
- `max_memory_bytes` est non nul et inférieur à la taille.

`NG_BRUTE_MAX_CELLS` vaut **25**. C'est une constante de compilation de `src/solvers/s0_brute/brute.c`, sans réglage à l'exécution. L'en-tête interne `brute.h` n'expose que la vtable `ngi_solver_brute`, comme le demande [PLAYBOOKS P1](../agents/PLAYBOOKS.md#p1--ajouter-un-solver) ; les tests utilisent la valeur 25 de cette spécification.

Avec `ng_dim` sur 32 bits, le produit `l × h` tient toujours sur 64 bits : `ckd_mul` ne peut pas y déborder, mais il reste employé comme pour tout calcul de taille (règle d'or 6).

## 5. Limites et comportements

- **Limites respectées** :
  - `max_memory_bytes` est contrôlée par `workspace_size`, avant toute allocation ;
  - `max_nodes` arrête l'énumération avant le nœud suivant, donc `nodes` vaut exactement `max_nodes` ;
  - `max_time_ms` est contrôlée tous les `NGI_BRUTE_TIME_CHECK` nœuds (4 096), pour ne pas lire l'horloge à chaque candidate. Le dépassement est d'au plus un intervalle ; l'horloge vient de l'infrastructure solver ([#10](https://github.com/lou-is-bea/nonogram-solver/issues/10)).
- **Grille d'entrée partiellement remplie** : les cases connues ne changent jamais (P-S4). Si elles contredisent les indices, aucune candidate ne passe et le résultat est `UNSOLVABLE`. Une grille déjà complète donne `k = 0` et un seul nœud.
- **Comptage de solutions** : supporté. S0 s'arrête dès que `max_solutions` solutions sont trouvées. Si l'énumération se termine avant, `solutions_found` est le nombre **exact** de solutions.
- **Graine** : ignorée. S0 n'a aucune source d'aléa. Sans limite de temps, nœuds, résultat et grille sont identiques d'une exécution à l'autre (NF-12, P-S5). Un arrêt par `max_time_ms` dépend par nature de la machine : le nombre de nœuds au moment de l'arrêt n'est pas reproductible.
- **Statistiques** : `solve` écrit `nodes` (candidates examinées), `solutions_found` et `line_solves = 0`. `elapsed_ns` et `workspace_bytes` sont remplis par `ng_solve` (#10).
- **Erreurs** (le solver peut être appelé directement par sa vtable, sans passer par `ng_solve`) :
  - `workspace_size` : `NG_ERR_INVALID_ARGUMENT` pour un puzzle, des options ou un `out` nuls, ou `max_solutions = 0` ; `NG_ERR_LIMIT` selon le §4.
  - `init` : `NG_ERR_INVALID_ARGUMENT` pour un puzzle ou des options nuls, `max_solutions = 0`, un espace de travail nul, désaligné ou plus petit que `workspace_size` pour ce puzzle et ces options. Il ne lit ni n'écrit rien au-delà de `ws_size`.
  - `solve` : `NG_ERR_INVALID_ARGUMENT` pour un argument nul, un espace de travail que `init` n'a pas préparé (l'état porte une marque posée par `init`), ou une grille dont les dimensions diffèrent de celles du puzzle ; ces contrôles précèdent toute écriture, donc la grille, les statistiques et le résultat restent inchangés. Une grille de mauvaise taille ne peut donc pas faire déborder `candidate[]`.
- **Résultats possibles** :

| Résultat | Condition | Grille `inout` | `solutions_found` |
|---|---|---|---|
| `SOLVED` | au moins une solution, et l'énumération est allée à son terme ou a atteint `max_solutions` | la première solution trouvée (la plus petite dans l'ordre du §2) | exact si `< max_solutions`, sinon égal à `max_solutions` (au moins autant de solutions) |
| `UNSOLVABLE` | énumération complète, aucune solution | inchangée | 0 |
| `LIMIT_REACHED` | `max_nodes` ou `max_time_ms` atteinte avant la fin | la première solution si `solutions_found ≥ 1`, sinon inchangée | borne inférieure |
| `INCOMPLETE` | jamais : S0 est complet | — | — |

## 6. Plan de test

**Budget de temps.** Une énumération de 25 cases inconnues examine jusqu'à `3,4 × 10⁷` candidates : environ 9 s sous Valgrind (G7) d'après la mesure de la revue de la spécification, au-delà du `.timeout` de 10 s des tests de solver (TESTING §4.1). Les tests sont donc répartis en deux groupes :

- **tests courants** (labels `unit` et `property`, donc G3 à G7, sanitizers et Valgrind compris) : au plus **16 cases inconnues** (`2^16` candidates) par résolution, avec le `.timeout` de 10 s ;
- **test exhaustif du corpus** (label `exhaustive`, Release seulement, ajouté à G3 par #11, sans sanitizer ni Valgrind, `.timeout` de 120 s) : résolution de tout le corpus `tiny` depuis une grille vide. C'est le critère de sortie de M4. La justesse mémoire du code exercé est déjà couverte par les tests courants, qui passent par les mêmes chemins sur de plus petites grilles.

**Cas.**

- [ ] Cas limites de [TESTING.md §4.2](../TESTING.md#42-cas-limites-obligatoires), dans la limite de 25 cases :
  - grille 1×1, vide puis pleine ;
  - ligne d'indice vide, ligne entièrement pleine, indices sans liberté de placement ;
  - grilles non carrées dans les deux sens, dont `1 × 16` et `16 × 1` ;
  - puzzle à solutions multiples (`checker_n2`, `diagonal_n2`) et puzzle insoluble (`crossed`).
- [ ] Dimensions hors limite, refusées par `workspace_size` avec `NG_ERR_LIMIT` sans allocation de grille : `1 × 26`, `26 × 1`, `6 × 5`, `1 × 10⁶`, `10⁶ × 1` et `70 000 × 70 000` (produit au-delà de 2³²). `NG_DIM_MAX` n'est pas testable ici : `ng_puzzle_create` refuse déjà `NG_DIM_MAX + 1`, et un puzzle de `NG_DIM_MAX` lignes demanderait environ 16 Gio d'indices.
- [ ] Erreurs du §5 : chaque argument nul, `max_solutions = 0`, espace de travail désaligné, trop petit d'un octet, non initialisé ; grille plus large, plus étroite, plus haute et plus basse que le puzzle (rien n'est écrit) ; espace de travail exactement à la taille de la formule (accepté).
- [ ] Propriétés applicables :
  - P-S1 sur des grilles aléatoires d'au plus **16 cases** ;
  - P-S4 : cases connues en entrée, y compris contradictoires ;
  - P-S5 : déterminisme des nœuds et de la grille, sans limite de temps ;
  - P-S6 : allocateur compteur à 0 pendant `solve`.

  Exclusions : P-S2 concerne S2 seulement. P-S3 (différentiel) n'a pas encore d'autre solver complet : il est remplacé à M4 par la comparaison avec les nombres de solutions documentés du corpus, et deviendra effectif avec S1.
- [ ] Revérification du corpus, annoncée par `tests/data/README.md` : unicité de chaque puzzle `tiny` (comptage avec `max_solutions = 2`, résultat 1, test exhaustif) ; nombre de solutions des puzzles `multi` d'au plus 25 cases ; `UNSOLVABLE` pour `crossed`.
- [ ] Aucune allocation pendant `solve` (P-S6).
- [ ] Tests spécifiques à l'algorithme :
  - ordre d'énumération : sur un puzzle à deux solutions, la solution renvoyée est la plus petite ;
  - nombre de nœuds d'une énumération complète égal à `2^k`, avec et sans cases connues ;
  - `max_nodes` : arrêt exact, et `LIMIT_REACHED` qui conserve une solution déjà trouvée ;
  - `max_time_ms` avec une horloge de test (horloge injectable de #10) ;
  - `max_memory_bytes` juste au-dessous et au-dessus de la formule ;
  - `max_solutions = 1` et `> 1`, avec un nombre exact quand l'énumération se termine ;
  - formule de `workspace_size` (par différences, voir le §4) ;
  - mutations ciblées : retenue de l'incrément, comparaison du dernier bloc d'une ligne, ligne d'indice vide, condition d'arrêt du comptage, contrôle des dimensions dans `solve`.

## 7. Mesures

À compléter à la livraison du solver (#11), avec le harnais de benchmark (#13).

| Corpus | Résolus | Temps médian | Pic mémoire | Commit et machine |
|---|---|---|---|---|
| tiny | | | | |
| small | hors limite (plus de 25 cases) | — | — | — |

## 8. Passage à l'échelle et parallélisation

- **Taille de grille maximale traitable** : 25 cases, par construction. La mémoire n'est pas la contrainte (moins de 1 Kio) : c'est le temps, en `2^N`.
- **Parties parallélisables** : l'énumération se découpe en plages indépendantes de candidates. On fixe les premiers chiffres binaires, et chaque thread énumère les suivants. Le comptage se fait alors par somme, et la première solution est la plus petite des premières solutions des plages. Rien de cela n'est prévu : S0 reste séquentiel.
- **Obstacles à une version distribuée** : aucun intérêt pratique. Au-delà de quelques dizaines de cases, `2^N` rend la force brute inutilisable quel que soit le nombre de nœuds de calcul.

## 9. Pistes d'amélioration

Volontairement non faites, pour garder S0 simple et donc fiable comme oracle :

- ne recontrôler que les lignes touchées par l'incrément (contrôle incrémental) ;
- élaguer dès qu'une rangée complète est fausse, au lieu d'énumérer toutes ses suites ;
- énumérer en code de Gray (une seule case change par candidate).

Ces optimisations sont précisément celles qui rendent S1 et les suivants plus rapides, et plus difficiles à prouver justes.

## 10. Décisions à approuver

Cette spécification fixe quatre points que la feuille de route ne tranche pas :

1. **Un nœud est une grille candidate examinée** : `max_nodes` et `nodes` en dépendent.
2. **`LIMIT_REACHED` pendant un comptage** conserve dans la grille la première solution trouvée, si elle existe. `solutions_found` est alors une borne inférieure. Le contrat d'ARCHITECTURE §6 ne garantit une grille complète que pour `SOLVED` : c'est une garantie en plus.
3. **`NG_BRUTE_MAX_CELLS` est une constante de compilation** (25), et non une option à l'exécution.
4. **Critère de sortie de M4 sur le comptage** : trois des cinq puzzles de `multi` dépassent 25 cases (`letters_ng_n2` 105, `smiley_n9` 100, `stripes_n6` 96). S0 ne peut donc compter que `checker_n2` et `diagonal_n2`. Proposition : le critère porte sur les puzzles de `multi` d'au plus 25 cases, et #11 ajoute au corpus deux ou trois puzzles à solutions multiples de cette taille, avec leur nombre de solutions dans le nom, comme les autres. Ce nombre est établi indépendamment de S0, par la même énumération indépendante que le reste du corpus (`tests/data/README.md`), puis revérifié par S0.

Une fois ces décisions prises, le texte de M4 dans la feuille de route sera aligné : énumération des `2^k` cases inconnues plutôt que `2^(largeur×hauteur)`, et `NG_BRUTE_MAX_CELLS` constante (25) plutôt que « 25 par défaut ».
