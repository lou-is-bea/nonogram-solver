# Documentation de nonogram-solver

## Parcours de lecture

**Vous rejoignez le projet (humain)**

1. [../README.md](../README.md) : vue d'ensemble
2. [REQUIREMENTS.md](REQUIREMENTS.md) : ce que le système doit faire
3. [ARCHITECTURE.md](ARCHITECTURE.md) : comment il est construit
4. [ROADMAP.md](ROADMAP.md) : où on en est
5. [../CONTRIBUTING.md](../CONTRIBUTING.md) : comment contribuer

**Vous êtes un agent IA** : commencez par [../AGENTS.md](../AGENTS.md), qui indique quoi lire selon la tâche.

## Index

| Document | Rôle | Normatif ? |
|---|---|---|
| [REQUIREMENTS.md](REQUIREMENTS.md) | Exigences identifiées (`F-xx`, `NF-xx`) | oui |
| [ARCHITECTURE.md](ARCHITECTURE.md) | Modules, dépendances, structures, API | oui |
| [ROADMAP.md](ROADMAP.md) | Étapes M0 à M9, livrables, critères de sortie, questions ouvertes | oui (périmètre) |
| [CODING_STANDARDS.md](CODING_STANDARDS.md) | Règles de code C23 | oui |
| [TESTING.md](TESTING.md) | Stratégie de test, portes de qualité, CI | oui |
| [FILE_FORMATS.md](FILE_FORMATS.md) | Formats d'entrée et de sortie | oui |
| [GLOSSARY.md](GLOSSARY.md) | Vocabulaire | non |
| [adr/](adr/README.md) | Décisions d'architecture | oui (une fois acceptées) |
| [solvers/](solvers/TEMPLATE.md) | Une spécification par solver | oui |
| [agents/PLAYBOOKS.md](agents/PLAYBOOKS.md) | Procédures pas à pas pour les agents | oui |
| [../.github/pull_request_template.md](../.github/pull_request_template.md) | Gabarit de PR | oui |

« Normatif » signifie que le code doit s'y conformer. Un écart constitue un défaut, soit du code, soit du document.

## Règles de maintenance de la documentation

- **Une information, un seul endroit.** Les autres documents renvoient par un lien plutôt que de recopier.
- La documentation est modifiée **dans la même PR** que le code qu'elle décrit.
- Chaque document normatif porte en tête la date de sa dernière revue. Un document qui n'a pas été relu depuis plus de 3 mois est relu au début de l'étape suivante.
- Les décisions ne se discutent pas dans les documents de référence : elles passent par un ADR.
