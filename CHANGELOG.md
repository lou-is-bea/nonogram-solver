# Changelog

Toutes les modifications notables de ce projet sont consignées dans ce fichier.

Le format suit [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) et le projet adhère au [versionnage sémantique](https://semver.org/lang/fr/).

## [Unreleased]

### Ajouté
- Documentation initiale du projet : README, AGENTS, CONTRIBUTING, exigences, architecture, feuille de route, normes de code, stratégie de test, formats de fichiers, glossaire, ADR 0001 à 0010 et gabarits.
- ADR-0009 : types sémantiques et support des grilles massives.
- ADR-0010 : lecture en flux et formats binaires `.ngp` / `.ngg`.
- Étape M9 de la feuille de route : grilles massives et parallélisme.
- `scripts/setup-wsl.sh` : installation de l'environnement Ubuntu (WSL2), clonage et import initial.
- `.gitignore` et `.gitattributes` (fins de ligne LF).

### Modifié
- ADR-0001 : passage de C11 à C23 (GCC ≥ 14, Clang ≥ 19), plateforme Linux uniquement.
- ADR-0003 : Criterion remplace Unity comme framework de tests.
- Retrait de toute cible Windows et MSVC (développement sous Ubuntu dans WSL2).
- Environnement de référence : Ubuntu 24.04 LTS avec GCC 14 et Clang 20 (paquets versionnés, compilateurs par défaut non utilisés).
