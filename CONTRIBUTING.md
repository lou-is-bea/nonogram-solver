# Contribuer à nonogram-solver

Ce guide décrit comment une modification passe d'une idée à `main`. Il s'applique aux contributeurs humains comme aux agents IA. Les agents suivent en plus [AGENTS.md](AGENTS.md).

---

## 1. Principe

**Rien n'entre dans `main` sans avoir été validé.** La validation comprend la pipeline automatique complète ([docs/TESTING.md](docs/TESTING.md)) **et** au moins une revue humaine.

## 2. Mise en place de l'environnement

Le développement se fait **exclusivement sous Linux**, avec **Ubuntu 24.04 LTS** dans WSL2 (environnement de référence). La chaîne de compilation de référence est GCC 14 et Clang 20, appelés explicitement par les presets CMake (`gcc-14`, `clang-20`), car les compilateurs par défaut de 24.04 (GCC 13, Clang 18) ne supportent pas complètement C23. Aucun support Windows natif n'est prévu.

**Installation automatique** : `bash scripts/setup-wsl.sh`, lancé depuis Ubuntu, installe les paquets ci-dessous, connecte `gh` à GitHub et clone le dépôt dans `~/dev/nonogram-solver`. Le script peut être relancé sans risque. Les étapes manuelles équivalentes suivent.

```sh
# Ubuntu 24.04 LTS (mêmes paquets versionnés sur 26.04)
sudo apt install build-essential gcc-14 libstdc++-14-dev clang-20 clang-19 clang-format-20 clang-tidy-20 \
                 libclang-rt-20-dev llvm-20 cmake ninja-build pkg-config libcriterion-dev \
                 valgrind cppcheck gcovr python3 git gh rsync
```

```sh
# Cloner dans le système de fichiers Linux de WSL, PAS sous /mnt/c
mkdir -p ~/dev && cd ~/dev
git clone <url> nonogram-solver && cd nonogram-solver
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

**Pourquoi pas sous `/mnt/c` ni dans OneDrive ?** Les accès aux fichiers Windows depuis WSL sont beaucoup plus lents (la compilation et les tests en pâtissent), les permissions et les liens symboliques y sont mal gérés, et la synchronisation OneDrive des dossiers `build/` provoque des conflits et des verrous. Le dépôt Git est la source de vérité. Une copie dans OneDrive n'est utile que pour la documentation, jamais comme dossier de travail pour compiler.

Éditeur : VS Code avec l'extension WSL, ou CLion en mode WSL, ouvert sur le dossier Linux.

Pour reproduire exactement la CI : `docker build -t nonogram-dev tools/docker && docker run --rm -v "$PWD:/src" -w /src nonogram-dev ./scripts/check.sh`.

## 3. Cycle de vie d'une modification

```
ticket ─▶ branche ─▶ tests + code ─▶ check.sh local ─▶ PR ─▶ CI ─▶ revue indépendante ─▶ revue humaine ─▶ squash merge ─▶ main
```

1. **Ticket.** Chaque travail correspond à un ticket qui précise l'objectif, l'étape de la feuille de route et les critères d'acceptation.
2. **Branche.** Le nom suit le format `<type>/<sujet-court>`, par exemple `feat/verify-report` ou `fix/parser-empty-line`.
3. **Développement guidé par les tests.** On écrit d'abord le test qui échoue, puis le code.
4. **Validation locale.** `./scripts/check.sh` doit réussir.
5. **Pull request.** Le gabarit [.github/pull_request_template.md](.github/pull_request_template.md) est obligatoire. Une PR correspond à un objectif et compte idéalement moins de 400 lignes modifiées, hors données de test.
6. **CI.** Toutes les portes bloquantes doivent être vertes.
7. **Revue indépendante.** Un relecteur qui n'est pas l'auteur (un agent IA démarrant sans contexte, ou une autre personne) applique [docs/agents/REVIEW.md](docs/agents/REVIEW.md) : il vérifie les affirmations de la PR, relance les portes que la CI ne couvre pas, éprouve les tests par des mutations et rend un verdict argumenté. L'auteur répond à chaque constat bloquant, majeur ou question.
8. **Revue humaine.** Au moins un humain approuve, au vu de la PR et de la revue indépendante. Une PR produite par un agent IA est toujours relue par un humain.
9. **Fusion.** On fusionne en *squash merge*. Le message final respecte les Conventional Commits.

## 4. Définition de « terminé »

Une modification est terminée quand **toutes** ces conditions sont réunies :

- [ ] Les critères d'acceptation du ticket sont remplis.
- [ ] Des tests couvrent le nouveau comportement et ses cas d'erreur.
- [ ] Toutes les portes bloquantes de [docs/TESTING.md §3](docs/TESTING.md#3-portes-de-qualité) sont vertes.
- [ ] Il n'y a aucune régression de benchmark au-delà des seuils, ou la régression est justifiée et acceptée dans la PR.
- [ ] L'API publique est documentée (Doxygen) et son impact est indiqué dans la PR.
- [ ] La documentation concernée et le `CHANGELOG.md` sont à jour.
- [ ] Un humain a approuvé la PR.

## 5. Messages de commit

On suit le format [Conventional Commits](https://www.conventionalcommits.org/), en anglais :

```
<type>(<scope>): <summary in imperative, ≤ 72 chars>

<body: why, not what>

<footer: BREAKING CHANGE: ..., Refs: #123>
```

- Types : `feat`, `fix`, `perf`, `refactor`, `test`, `docs`, `build`, `ci`, `chore`.
- Scopes : `core`, `io`, `render`, `verify`, `solver/<nom>`, `cli`, `bench`, `tests`, `docs`.

## 6. Revue de code : à quoi le relecteur fait attention

1. **Justesse** : invariants, cas limites (grille 1×1, ligne vide, indice égal à la longueur de la ligne), débordements entiers (calculs en 32 bits sur des index de case, absence de `ckd_*`).
2. **Mémoire** : propriété de chaque pointeur, libération sur chaque chemin d'erreur, absence d'allocation dans les boucles chaudes.
3. **Architecture** : respect des couches et des ADR, pas de couplage caché.
4. **Tests** : les tests échoueraient-ils si le code était faux ? Les chemins d'erreur sont-ils couverts ?
5. **Lisibilité** : noms, taille des fonctions, commentaires qui expliquent le *pourquoi*.

## 7. Décisions d'architecture

Toute décision structurante (format, dépendance, représentation des données, interface publique, outil obligatoire) fait l'objet d'un **ADR** dans [docs/adr/](docs/adr/README.md), avec le statut `Proposé`. Elle n'est appliquée qu'une fois passée au statut `Accepté` par un responsable humain.

## 8. Versions

Le projet suit le [versionnage sémantique](https://semver.org/lang/fr/) à partir de la version 0.1.0 (fin de M3). Le [CHANGELOG.md](CHANGELOG.md) suit le format [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/).
