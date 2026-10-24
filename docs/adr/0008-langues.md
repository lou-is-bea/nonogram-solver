# ADR-0008 : Documentation en français, code en anglais

- **Statut** : Accepté
- **Date** : 2026-10-24

## Contexte

L'équipe est francophone. Le code doit cependant rester accessible au plus grand nombre (contributeurs, outils, agents IA) et cohérent avec la bibliothèque standard et l'écosystème C.

## Décision

- **Français** : les documents de `docs/`, le README, CONTRIBUTING et AGENTS, ainsi que les tickets et les descriptions de PR.
- **Anglais** : les identifiants, les commentaires de code, les commentaires Doxygen, les messages de commit, les messages d'erreur de la bibliothèque et les sorties de la CLI.
- Le [glossaire](../GLOSSARY.md) fait la correspondance entre les termes français et anglais.

## Conséquences

- **Positives** : la documentation de conception est confortable pour l'équipe, et le code reste standard et réutilisable.
- **Négatives** : deux langues coexistent. Le glossaire limite les écarts de vocabulaire.

## Critères de réexamen

L'arrivée de contributeurs non francophones. Il faudrait alors traduire `docs/` en anglais.
