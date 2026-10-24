# ADR-0004 : Allocateur injectable et espace de travail pré-alloué

- **Statut** : Accepté
- **Date** : 2026-10-24
- **Exigences liées** : NF-03, NF-04, NF-05, NF-07

## Contexte

La maîtrise de la mémoire est la raison même du choix du C. Il faut pouvoir **mesurer** la mémoire (pic, nombre d'allocations), la **borner** (limite passée aux solvers), **tester les échecs d'allocation** et éviter le coût des allocations dans les boucles chaudes.

## Options étudiées

- **`malloc` et `free` directs** : simple, mais il devient impossible de mesurer, de borner ou d'injecter des échecs sans outil externe.
- **Pool ou arène globale** : performant, mais crée un état global (contraire à NF-08).
- **Allocateur injectable et une allocation par objet** : chaque fonction qui alloue reçoit un `const ng_allocator *`. Les solvers déclarent la taille de leur espace de travail et ne reçoivent qu'un bloc.

## Décision

1. Toute allocation passe par `ng_allocator { alloc(ctx, size, align), free(ctx, ptr, size), ctx }`. La taille est repassée à `free` pour permettre le comptage exact et les arènes.
2. `ng_allocator_default()` enveloppe `malloc` et `free`. C'est le seul endroit du code où ces fonctions sont appelées.
3. Un objet correspond à une allocation : le puzzle, la grille et l'espace de travail d'un solver sont des blocs uniques, découpés en interne selon une mise en page calculée avec des opérations protégées contre les débordements.
4. Les solvers exposent `workspace_size()`. `init()` et `solve()` n'allouent pas.
5. En test, on utilise un allocateur compteur (pic, octets encore alloués, nombre d'appels) et un allocateur défaillant (échec à la n-ième allocation).

## Conséquences

- **Positives** : l'empreinte est mesurable, prévisible et bornée. La limite mémoire se contrôle avant de démarrer. L'absence de fuites se vérifie dans chaque test.
- **Négatives** : un solver doit connaître sa taille maximale à l'avance. Pour une recherche, la profondeur est bornée par `largeur × hauteur` (une hypothèse par case au plus), ce qui permet de dimensionner la pile et le *trail*.
- **Actions** : `allocator.h`, `tests/support/`, une règle de recherche dans `check.sh` qui interdit `malloc` hors de `allocator.c`.

## Critères de réexamen

Si un futur solver a un besoin mémoire qu'on ne peut pas borner à l'avance (par exemple une mémoïsation non bornée). Il faudra alors un cache de taille fixe ou un nouvel ADR.
