# ADR-0003 : Criterion comme framework de tests unitaires

- **Statut** : Accepté (2026-10-25)
- **Date** : 2026-10-25 (révision de la version du 2026-10-24, qui proposait Unity)
- **Exigences liées** : NF-06, NF-09

## Contexte

Il faut un framework de test en C, moderne, adapté à un environnement **Linux uniquement**, qui reste compatible avec les sanitizers, Valgrind et la mesure de couverture. Il doit aussi permettre d'écrire facilement les nombreux cas paramétrés du projet (cas limites, fichiers du corpus).

## Options étudiées

- **Unity** : minimal et très portable, mais l'enregistrement des tests est manuel, il n'y a ni isolation ni tests paramétrés. Sa portabilité n'est plus un critère.
- **cmocka** : il offre des *mocks*, inutiles ici puisque les dépendances sont injectées (allocateur, `ng_writer`).
- **Criterion** (licence MIT, paquet `libcriterion-dev` dans Ubuntu) :
  - les tests s'enregistrent automatiquement (`Test(suite, nom)`), sans `main()` à écrire ;
  - **chaque test tourne dans un processus séparé** : un plantage (segfault, `abort`) est rapporté comme un échec du test sans interrompre la suite ;
  - les tests attendus en échec (`.signal = SIGABRT`, `.exit_code`) permettent de tester les assertions internes en Debug ;
  - délai maximal par test (`.timeout`), utile pour détecter les boucles infinies des solvers ;
  - tests paramétrés (`ParameterizedTest`) et théories (`Theory`) ;
  - fixtures par suite (`.init`, `.fini`) ;
  - sorties TAP, JUnit XML et JSON pour la CI, exécution parallèle (`-j`) et filtrage (`--filter`).

## Décision

Le projet utilise **Criterion ≥ 2.4**, installé comme paquet système (`libcriterion-dev`), détecté par CMake via `pkg-config` et inclus comme en-tête **SYSTEM**, ce qui évite que ses en-têtes déclenchent nos options de warnings strictes. Chaque fichier `tests/unit/test_<module>.c` produit un exécutable Criterion, enregistré dans CTest avec ses labels.

Les fixtures de suite (`.init` / `.fini`) installent un **allocateur compteur** et font échouer le test s'il reste de la mémoire allouée à la fin.

## Conséquences

- **Positives** : les tests sont plus concis et plus expressifs, les plantages sont isolés, les cas limites s'écrivent en tableaux paramétrés et les rapports CI sont natifs.
- **Négatives et risques** :
  - Criterion devient une dépendance système. Sa version est figée dans l'image Docker de référence ;
  - l'isolation par `fork()` impose de lancer **Valgrind avec `--trace-children=yes`**. Les éventuels faux positifs internes à Criterion sont neutralisés dans un fichier de suppressions unique, `tests/valgrind/criterion.supp`, approuvé une fois par un humain. Il ne doit contenir **aucune** suppression qui porte sur du code du projet ;
  - pour le débogage pas à pas, on lance l'exécutable de test avec l'option `--debug=gdb` de Criterion (la procédure exacte est documentée dans `tests/README.md` à M0).
- **Actions** : image Docker (M0), `tests/CMakeLists.txt` (`pkg_check_modules(CRITERION REQUIRED criterion)`), premier test `test_version.c`, mise à jour de [TESTING.md](../TESTING.md).

## Critères de réexamen

Si Criterion cesse d'être maintenu ou disparaît des dépôts Ubuntu LTS.
