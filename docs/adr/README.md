# Décisions d'architecture (ADR)

Un ADR consigne **une** décision structurante : son contexte, les options étudiées, le choix retenu et ses conséquences.

**Cycle de vie** : `Proposé` → `Accepté` ou `Rejeté` → (`Déprécié` | `Remplacé par ADR-XXXX`).

- Un ADR au statut `Proposé` peut être modifié librement jusqu'à son acceptation.
- Un ADR `Accepté` n'est plus réécrit sur le fond. Une décision qui change donne lieu à un nouvel ADR, et l'ancien passe à `Remplacé par ADR-XXXX`. Seules les corrections de forme ou de contexte sont permises, et elles sont signalées par une mention « révisé le … » dans l'en-tête.
- Seul un responsable humain fait passer un ADR à `Accepté`.

**Créer un ADR** : copier [TEMPLATE.md](TEMPLATE.md) vers `NNNN-titre-court.md`, en prenant le numéro suivant, et l'ajouter au tableau ci-dessous.

| # | Titre | Statut |
|---|---|---|
| [0001](0001-langage-c23.md) | C23 comme langage du moteur (Linux, GCC ≥ 14, Clang ≥ 19) | Accepté (révisé le 2026-10-25) |
| [0002](0002-build-cmake.md) | CMake, CTest et presets comme système de build | Accepté (révisé le 2026-10-25) |
| [0003](0003-framework-de-test-criterion.md) | Criterion comme framework de tests unitaires | Accepté (révisé le 2026-10-25) |
| [0004](0004-gestion-memoire.md) | Allocateur injectable et espace de travail pré-alloué | Accepté |
| [0005](0005-architecture-modulaire.md) | Bibliothèque en couches et solvers enfichables | Accepté |
| [0006](0006-formats-de-fichiers.md) | Formats `.non` (entrée) et `.sol` (grille) | Accepté |
| [0007](0007-verificateur-oracle.md) | Vérificateur indépendant utilisé comme oracle | Accepté |
| [0008](0008-langues.md) | Documentation en français, code en anglais | Accepté |
| [0009](0009-types-et-grilles-massives.md) | Types dédiés et support des grilles massives | Accepté |
| [0010](0010-formats-binaires-et-lecture-en-flux.md) | Formats binaires et lecture en flux | Accepté |
