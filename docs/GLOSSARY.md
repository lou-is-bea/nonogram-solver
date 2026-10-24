# Glossaire

| Terme | Anglais (dans le code) | Définition |
|---|---|---|
| Nonogramme | nonogram | Puzzle logique : il faut noircir des cases d'une grille selon des indices donnés pour chaque rangée et chaque colonne. On l'appelle aussi picross, hanjie ou griddler. |
| Puzzle | puzzle | La définition du problème : les dimensions et tous les indices. Il est immuable. |
| Grille | grid | Une affectation (partielle ou complète) d'un état à chaque case. |
| Case | cell | L'unité de la grille. Son état est `UNKNOWN`, `EMPTY` ou `FILLED`. |
| Rangée | row | Une ligne horizontale de la grille. |
| Colonne | column | Une ligne verticale de la grille. |
| Ligne | line | Une rangée ou une colonne, sans distinction. Elle a un identifiant unifié : `0..h-1` pour les rangées, `h..h+w-1` pour les colonnes. |
| Indice | clue | La suite des longueurs de blocs d'une ligne, par exemple `3 1 2`. |
| Bloc | run (ou block) | Une suite maximale de cases pleines consécutives dans une ligne. |
| Placement | placement | Une position de tous les blocs d'une ligne qui respecte ses indices. |
| Solveur de ligne | line solver | Un algorithme qui, à partir des indices et de l'état partiel d'une ligne, déduit les cases forcées ou détecte une contradiction. |
| Propagation | propagation | L'application répétée du solveur de ligne aux lignes modifiées, jusqu'à ce que plus rien ne change. |
| Point fixe | fixpoint | L'état où la propagation ne déduit plus aucune nouvelle case. |
| Contradiction | contradiction | Un état partiel qui n'admet aucun placement pour au moins une ligne. |
| Recherche, ou backtracking | search / backtracking | L'exploration arborescente : on fait une hypothèse, puis on revient en arrière en cas de contradiction. |
| Nœud (de recherche) | node | Une hypothèse posée pendant la recherche. Le nombre de nœuds sert de mesure de l'effort. Employé seul, « nœud » a toujours ce sens. |
| Nœud de calcul | compute node ; `rank` ou `worker` dans le code | Une machine d'un cluster de calcul parallèle. On ne l'abrège jamais en « nœud », pour éviter la confusion avec les nœuds de recherche. |
| Grille massive | massive grid | Une grille dont la taille impose des précautions spécifiques : plus de 2³² cases, ou trop grande pour la mémoire d'une seule machine. Typiquement à partir de 10⁴ × 10⁴. |
| Copie de ligne | line buffer | Le tampon contigu dans lequel un solver charge une ligne (`ng_grid_load_line`) puis la réécrit (`ng_grid_store_line`). C'est l'unité de travail et d'échange. |
| Types sémantiques | semantic types | Les types de `types.h` (`ng_dim`, `ng_cell_index`, …), qui portent le sens d'un entier et sa largeur. |
| Lecture en flux | streaming | Une lecture par blocs de taille fixe, sans charger le fichier entier en mémoire. |
| Trail | trail | La pile des modifications de cases, qui permet d'annuler les hypothèses sans copier la grille. |
| Sondage | probing | Essayer une valeur pour une case, propager, et en déduire la valeur opposée si l'essai mène à une contradiction. |
| Solver complet | complete solver | Un solver qui trouve toujours une solution si elle existe, en l'absence de limite. |
| Espace de travail | workspace | La mémoire d'un solver, allouée une seule fois avant la résolution. |
| Oracle | oracle | La référence de vérité utilisée par les tests. Ici, c'est le vérificateur. |
| Porte de qualité | quality gate | Une vérification automatique qui doit réussir pour qu'une modification soit fusionnée. |
| Sortie de référence | golden file | La sortie attendue, relue par un humain, à laquelle on compare la sortie réelle. |
| ADR | Architecture Decision Record | Un document court qui consigne une décision d'architecture et ses raisons. |
