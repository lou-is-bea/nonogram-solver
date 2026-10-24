# ADR-0001 : C23 comme langage du moteur

- **Statut** : Accepté (2026-10-25)
- **Date** : 2026-10-25 (révision de la version du 2026-10-24, qui proposait C11 ; environnement de référence précisé le 2026-10-25 : Ubuntu 24.04, la décision est inchangée)
- **Exigences liées** : NF-01, NF-02, NF-03

## Contexte

Le cahier des charges impose le C pour le moteur, afin de maîtriser finement la consommation mémoire. Le développement et l'exécution se font **exclusivement sous Linux** : Ubuntu sous WSL2 pour le poste de développement, Linux pour la CI et les futurs clusters de calcul. Aucune contrainte de compatibilité avec une autre plateforme ou un autre compilateur ne s'applique. On peut donc choisir la révision la plus récente du standard qui soit supportée par GCC et Clang.

## Options étudiées

- **C11 ou C17** : supportés partout, mais ils obligent à recréer avec des macros ce que C23 offre de façon standard (`nodiscard`, arithmétique vérifiée, manipulation de bits).
- **C23** (ISO/IEC 9899:2024) : supporté par GCC ≥ 14 et Clang ≥ 19, tous deux disponibles sur Ubuntu 24.04 et 26.04 LTS. Il apporte des fonctionnalités directement utiles au projet :
  - `<stdckdint.h>` (`ckd_add`, `ckd_mul`) pour des calculs de taille protégés contre les débordements, indispensables avec des grilles massives ([ADR-0009](0009-types-et-grilles-massives.md)) ;
  - `<stdbit.h>` (`stdc_count_ones`, `stdc_trailing_zeros`…) pour les représentations en bitsets ;
  - les attributs standard `[[nodiscard]]`, `[[maybe_unused]]` et `[[fallthrough]]` ;
  - `constexpr` pour des constantes typées, à la place des `#define` ;
  - les énumérations à type sous-jacent fixe (`enum ng_cell : uint8_t`) ;
  - `nullptr`, `static_assert`, et `bool`, `true` et `false` comme mots-clés.
- **gnu23** (C23 avec les extensions GNU) : plus permissif, mais moins portable entre GCC et Clang et moins analysable.

## Décision

Le moteur est écrit en **C23 strict** (`-std=c23 -Wpedantic`), sans extension GNU. Les compilateurs supportés sont **GCC ≥ 14** et **Clang ≥ 19**, sous Linux uniquement. L'environnement de référence est **Ubuntu 24.04 LTS** avec **GCC 14 et Clang 20** (paquets `gcc-14` et `clang-20`, appelés explicitement par les presets CMake), sous WSL2 ou dans le conteneur de la CI. Les compilateurs par défaut de 24.04 (GCC 13, Clang 18) ne sont pas utilisés.

Les fonctionnalités C23 dont l'usage est encadré sont listées dans [CODING_STANDARDS.md §1](../CODING_STANDARDS.md#1-langage-et-compilation). En particulier, l'inférence de type `auto`, `_BitInt` et `#embed` ne sont pas utilisés sans justification.

## Conséquences

- **Positives** : moins de macros maison, une arithmétique de taille sûre et standard, des intrinsèques de bits portables, une intention plus lisible (attributs, `constexpr`, énumérations typées).
- **Négatives et risques** :
  - certains outils d'analyse (clang-tidy, cppcheck) ont un support C23 plus récent et parfois partiel. On fige donc leurs versions dans l'image Docker de référence, et toute exception est documentée dans `.clang-tidy` ou `check.sh` ;
  - `<stdbit.h>` est fourni par la glibc à partir de la version 2.39 (Ubuntu ≥ 24.04).
- **Actions** : options de compilation dans le CMake racine, vérification dans CMake qu'un compilateur C23 est disponible, image Docker de référence (M0).

## Critères de réexamen

Si une plateforme cible (un cluster de calcul, par exemple) ne dispose pas d'un compilateur C23. Les compilateurs récents s'installent généralement en espace utilisateur (modules, Spack), ce qui rend ce cas peu probable.
