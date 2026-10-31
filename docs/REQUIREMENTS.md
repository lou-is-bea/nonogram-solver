# Exigences

> Dernière revue : 2026-10-25. Statut : **à valider par les responsables du projet.**

Chaque exigence porte un identifiant stable. Les tickets, les PR et les tests y font référence (par exemple `Refs: F-12`). Le niveau d'obligation suit la RFC 2119 : **DOIT**, **DEVRAIT**, **PEUT**.

---

## 1. Périmètre

**Dans le périmètre**

- Les nonogrammes noir et blanc rectangulaires.
- Une bibliothèque C et un outil en ligne de commande, pour Linux.
- Des grilles de toute taille, jusqu'aux grilles massives destinées aux solvers parallèles (voir NF-14 et NF-15).

**Hors périmètre (pour l'instant)**

- Les nonogrammes en couleur.
- Les grilles triangulaires ou hexagonales.
- Une interface graphique.
- Windows et macOS comme plateformes d'exécution (le développement se fait sous Ubuntu dans WSL2).
- La génération de grilles « agréables » pour un humain.

Les nonogrammes en couleur pourront être ajoutés plus tard. L'architecture ne doit pas les rendre impossibles (voir NF-11).

## 2. Exigences fonctionnelles

### Modèle et entrées-sorties (M1)

| ID | Exigence |
|---|---|
| F-01 | Le système DOIT représenter un puzzle de dimensions `largeur × hauteur`, avec `1 ≤ largeur, hauteur ≤ NG_DIM_MAX` (2³¹ − 1), et ses indices de lignes et de colonnes. Le nombre de cases n'est limité que par la mémoire disponible ([ADR-0009](adr/0009-types-et-grilles-massives.md)). |
| F-02 | Le système DOIT représenter une grille dont chaque case est dans l'un des trois états `UNKNOWN`, `EMPTY` ou `FILLED`. |
| F-03 | Le système DOIT lire un puzzle au format `.non` ([FILE_FORMATS.md](FILE_FORMATS.md)) depuis toute source `ng_reader` (fichier, tampon mémoire). |
| F-04 | En cas d'entrée invalide, le système DOIT renvoyer une erreur qui indique la ligne du fichier et la nature du problème, sans plantage et sans fuite mémoire. |
| F-05 | Le système DOIT détecter les puzzles trivialement incohérents : une somme d'indices et d'espacements supérieure à la longueur de la ligne, ou un total de cases noires différent entre les lignes et les colonnes. |
| F-06 | Le système DOIT lire et écrire une grille au format `.sol`. |
| F-07 | Le système DEVRAIT lire et écrire les formats binaires `.ngp` (puzzle) et `.ngg` (grille), et proposer les commandes `convert` et `generate` (M9, [ADR-0010](adr/0010-formats-binaires-et-lecture-en-flux.md)). |
| F-08 | Toute lecture DOIT se faire en flux, par blocs de taille fixe, sans charger le fichier entier et sans limite de taille de fichier ou de longueur de ligne. |

### Affichage (M2)

| ID | Exigence |
|---|---|
| F-10 | Le système DOIT afficher un puzzle en texte, avec les indices de colonnes au-dessus et les indices de lignes à gauche. |
| F-11 | Le système DOIT afficher une grille complète ou partielle, en distinguant les trois états des cases. |
| F-12 | L'affichage DOIT pouvoir être écrit dans un flux (`FILE*`) ou dans un tampon mémoire, pour pouvoir être testé. Au-delà d'une taille configurable, la CLI DOIT afficher un résumé au lieu de la grille. |
| F-13 | L'affichage DEVRAIT proposer un jeu de caractères ASCII (par défaut) et un jeu de caractères Unicode. |

### Vérification (M3)

| ID | Exigence |
|---|---|
| F-20 | Le système DOIT déterminer si une grille est une solution valide d'un puzzle, c'est-à-dire si chaque ligne et chaque colonne correspond exactement à ses indices. |
| F-21 | La vérification DOIT distinguer trois cas : `VALID`, `INCOMPLETE` (au moins une case `UNKNOWN`) et `INVALID`. Dans le cas `INVALID`, elle DOIT indiquer la première ligne ou colonne fautive, ses indices attendus et les blocs observés. |
| F-22 | Le vérificateur DOIT être indépendant de tout solver ([ADR-0007](adr/0007-verificateur-oracle.md)). |

### Résolution (M4 à M8)

| ID | Exigence |
|---|---|
| F-30 | Le système DOIT proposer plusieurs solvers derrière une interface unique, sélectionnables par leur nom. |
| F-31 | Un solver DOIT renvoyer l'un des résultats `SOLVED`, `UNSOLVABLE` (contradiction prouvée), `INCOMPLETE` (le solver est bloqué sans conclure) ou `LIMIT_REACHED` (limite de temps, de nœuds ou de taille atteinte). |
| F-32 | Toute grille renvoyée avec le résultat `SOLVED` DOIT être `VALID` selon le vérificateur. |
| F-33 | Un solver DOIT respecter les limites qu'on lui passe : nombre maximal de nœuds, temps maximal et mémoire maximale. |
| F-34 | Un solver DEVRAIT fournir des statistiques : nombre de nœuds explorés, nombre de résolutions de lignes, durée et pic mémoire. |
| F-35 | Les solvers complets (S0, S1, S3 et suivants) DEVRAIENT pouvoir compter les solutions jusqu'à un plafond N, pour détecter les puzzles à solutions multiples. |
| F-36 | Un solver de propagation incomplet (S2) DOIT renvoyer la grille partielle déduite, même s'il ne conclut pas. |

### Interface en ligne de commande

| ID | Exigence |
|---|---|
| F-40 | La commande `nonogram` DOIT proposer les sous-commandes `show`, `solve`, `verify`, `list-solvers` et, depuis M4, `bench`, qui s'appuie sur le harnais `ng_bench` (décision du 2026-10-31). |
| F-41 | Les codes de sortie DOIVENT être stables et documentés : `0` succès ou solution valide, `1` puzzle insoluble, solution invalide ou incomplète, `2` usage incorrect, `3` erreur d'entrée, `4` erreur interne. |
| F-42 | `solve --stats` DEVRAIT afficher les statistiques au format texte ou JSON (`--format=json`). |

## 3. Exigences non fonctionnelles

| ID | Catégorie | Exigence |
|---|---|---|
| NF-01 | Langage | Le moteur DOIT être écrit en C23 standard, sans extension de compilateur ([ADR-0001](adr/0001-langage-c23.md)). |
| NF-02 | Plateforme | Le code DOIT compiler sans warning avec GCC ≥ 14 et Clang ≥ 19 sous Linux. L'environnement de référence est Ubuntu 24.04 LTS avec GCC 14 et Clang 20 (WSL2 et conteneur de CI). |
| NF-03 | Mémoire | Toute allocation DOIT passer par un allocateur injectable ([ADR-0004](adr/0004-gestion-memoire.md)). |
| NF-04 | Mémoire | Un solver NE DOIT PAS allouer de mémoire pendant sa boucle de résolution. Son espace de travail est alloué à l'initialisation et sa taille est calculable à l'avance. |
| NF-05 | Mémoire | La mémoire d'un puzzle et de sa grille DOIT être en `O(largeur × hauteur + nombre d'indices)`, avec une formule exacte documentée. L'empreinte de chaque solver DOIT être documentée dans sa spécification. |
| NF-06 | Robustesse | Aucune fuite, aucun accès hors limites et aucun comportement indéfini, vérifiés par ASan, UBSan et Valgrind. |
| NF-07 | Robustesse | Tout échec d'allocation DOIT être traité proprement : erreur renvoyée, aucune fuite, objets laissés dans un état cohérent. |
| NF-08 | Concurrence | La bibliothèque NE DOIT PAS avoir d'état global mutable. Des objets distincts DOIVENT pouvoir être utilisés en parallèle depuis plusieurs threads. |
| NF-09 | Qualité | La couverture de lignes DOIT atteindre au moins 90 % sur `core`, `io` et `verify`, et au moins 85 % sur `solvers` ([TESTING.md](TESTING.md)). |
| NF-10 | Performance | Chaque solver DOIT être mesuré par un benchmark reproductible. Une régression de plus de 10 % (temps médian ou pic mémoire) bloque la fusion, sauf justification acceptée. |
| NF-11 | Évolutivité | Ajouter un solver DOIT se limiter à un nouveau dossier dans `src/solvers/`, une entrée dans le registre, ses tests et sa spécification. |
| NF-12 | Déterminisme | À entrée et options identiques, un solver DOIT produire le même résultat et les mêmes statistiques de nœuds. Toute source d'aléa est pilotée par une graine. |
| NF-13 | Lisibilité | Le code suit [CODING_STANDARDS.md](CODING_STANDARDS.md), ce qui est vérifié automatiquement par clang-format et clang-tidy. |
| NF-14 | Passage à l'échelle | Les types du modèle DOIVENT supporter des grilles massives (index de case sur 64 bits, dimensions jusqu'à 2³¹ − 1), et tout calcul de taille DOIT être protégé contre les débordements ([ADR-0009](adr/0009-types-et-grilles-massives.md)). |
| NF-15 | Parallélisme | L'architecture DOIT permettre la parallélisation en mémoire partagée puis distribuée (plusieurs centaines de nœuds de calcul) sans refonte du modèle de données : puzzle immuable, accès par lignes, espace de travail par instance, déterminisme ([ARCHITECTURE.md §10](ARCHITECTURE.md#10-préparation-au-parallélisme-massif)). |

## 4. Objectifs de performance indicatifs

Ces objectifs seront ajustés une fois le corpus de benchmark constitué (M4).

| Solver | Objectif |
|---|---|
| S0 (force brute) | Grilles jusqu'à 25 cases. Il sert d'oracle sur les très petites grilles. |
| S1 (backtracking par lignes) | Grilles 10×10 en moins d'une seconde. |
| S2 (propagation) | Résout les grilles « solubles ligne par ligne » 30×30 en moins de 10 ms. |
| S3 (ligne exacte avec recherche) | Résout 99 % du corpus « large » (jusqu'à 100×100) en moins d'une seconde chacun. |
| Solvers parallèles (M9) | Objectifs d'accélération et tailles de grilles à fixer une fois l'architecture cible connue (question Q6 de la [feuille de route](ROADMAP.md#questions-ouvertes)). |
