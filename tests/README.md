# Tests

Stratégie, conventions et portes de qualité : voir [docs/TESTING.md](../docs/TESTING.md).

## Lancer les tests

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev                                   # tous les tests
ctest --preset dev -L unit                           # un label : unit, integration, property
ctest --preset dev -R version                        # par nom CTest
./build/dev/tests/unit/test_version --filter 'version_string/*'   # par nom Criterion
```

Sous sanitizers : `cmake --preset asan && cmake --build --preset asan && ctest --preset asan` (même chose avec `ubsan`).

## Ajouter un fichier de test

1. Crée `tests/unit/test_<module>.c`, avec `TestSuite(<module>_<unité>, .init = ng_test_setup, .fini = ng_test_teardown);` pour chaque suite.
2. Enregistre-le dans [CMakeLists.txt](CMakeLists.txt) : `ng_add_unit_test(<module> LIBS ng_<module>)`. Le label `unit` et la détection des rapports de sanitizers sont ajoutés automatiquement.

## Détection des fuites et des erreurs mémoire

Criterion exécute chaque test dans un processus fils. LeakSanitizer signale les fuites à la sortie de ce processus, **après** que Criterion a enregistré le résultat du test : lancé seul, l'exécutable de test affiche le rapport mais renvoie 0. C'est CTest qui fait échouer le test, car tout test dont la sortie contient un rapport de sanitizer (`ERROR: AddressSanitizer`, `ERROR: LeakSanitizer`, `runtime error:`) est en échec (`FAIL_REGULAR_EXPRESSION`). Passe donc toujours par `ctest` pour juger un résultat sous sanitizers.

À partir de M1, la fixture commune s'appuie aussi sur l'allocateur compteur et fait échouer le test qui laisse de la mémoire allouée par la bibliothèque.

## Déboguer un test pas à pas

Criterion peut lancer le test dans un serveur gdb (port 1234 par défaut) :

```sh
./build/dev/tests/unit/test_version --filter 'version_string/matches_header_components' --debug=gdb
```

Puis, dans un autre terminal :

```sh
gdb ./build/dev/tests/unit/test_version -ex 'target remote localhost:1234' -ex 'continue'
```

Évite `--debug=idle`, qui attend indéfiniment un débogueur.
