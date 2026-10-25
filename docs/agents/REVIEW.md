# Politique de revue des pull requests

> Dernière revue : 2026-10-25. Ce document est **normatif**. Il complète [AGENTS.md](../../AGENTS.md) et [CONTRIBUTING.md §3](../../CONTRIBUTING.md#3-cycle-de-vie-dune-modification) : toute PR reçoit, avant la revue humaine, une **revue indépendante** menée selon cette politique. Il s'adresse à l'agent relecteur (humain ou IA) et à l'auteur qui lui répond.

---

## 1. Principes

1. **Indépendance.** Le relecteur n'est pas l'auteur. Un agent IA relecteur démarre sans contexte : il n'a accès ni à la conversation ni aux notes de l'auteur, seulement au dépôt, à la PR et à la CI. Il ne relit jamais une PR à laquelle il a contribué.
2. **Scepticisme.** La description de la PR est une liste d'**affirmations à vérifier**, pas une preuve. « Vérifié », « couverture de 97 % », « médiane de 0,23 ms », « aucun test affaibli » : chaque affirmation qui compte pour le verdict est recontrôlée, ou signalée comme non vérifiée.
3. **Regard critique.** Le relecteur cherche ce qui manque autant que ce qui est faux : cas limites non testés, tests qui passeraient avec un code faux, chemins d'erreur oubliés, documentation en retard, choix discutables. Il conteste un choix de conception s'il le juge mauvais, même s'il est cohérent avec le reste de la PR, en le classant à sa juste gravité.
4. **Preuves.** Chaque constat cite un fichier et une ligne, une commande et sa sortie, ou un scénario concret qui produit un résultat faux. Un soupçon sans preuve est une **question**, pas un défaut.
5. **Lecture seule.** Le relecteur ne pousse aucun commit et ne modifie pas la PR. Il propose ; l'auteur corrige.
6. **Décision humaine.** Le verdict du relecteur éclaire la décision, il ne la remplace pas : la fusion reste approuvée par un humain ([CONTRIBUTING.md §3](../../CONTRIBUTING.md#3-cycle-de-vie-dune-modification)).

## 2. Périmètre

- Le relecteur examine le **diff propre de la PR** : `git diff <base>...<head>`. Pour une PR empilée, la base est la branche de la PR précédente, pas `main`.
- Il juge la PR **par rapport à l'étape de la feuille de route** qu'elle déclare : livrables, critères de sortie, et rien au-delà (une fonctionnalité hors étape est un constat).
- Un défaut qui vient d'une PR antérieure de la pile est signalé, en indiquant la PR d'origine ; il ne bloque la PR examinée que s'il en fausse le contenu.

## 3. Procédure

### 3.1 Préparer

1. Lire les documents obligatoires d'[AGENTS.md §2](../../AGENTS.md#2-documents-à-lire-avant-de-travailler) (normes et stratégie de test), puis ceux que la PR touche : ARCHITECTURE, ADR concernés, FILE_FORMATS, ROADMAP (étape déclarée).
2. Obtenir le code de la PR dans un espace de travail propre (worktree dédié ou clone), au **SHA exact** de la tête de la PR : `git fetch origin && git checkout --detach <head-sha>`.
3. Lire la description de la PR et en extraire la liste des affirmations vérifiables.
4. Lire le diff en entier. Pour une PR volumineuse, lire d'abord les en-têtes publics et les tests, puis l'implémentation.

### 3.2 Vérifier les faits

1. **CI** : l'exécution de la CI sur le SHA de tête est verte (`gh pr checks <n>`). Une CI rouge ou absente est un constat bloquant, sauf justification écrite dans la PR.
2. **Portes que la CI ne couvre pas** : les relancer localement. Au minimum `./scripts/check.sh --only G7,G8,G9` tant que la CI n'a pas de tâches Valgrind, couverture et fuzzing ([TESTING.md §5](../TESTING.md#5-intégration-continue)). Relancer aussi toute porte dont le résultat est contesté ou paraît improbable.
3. **Chiffres annoncés** : recalculer ce qui peut l'être à coût raisonnable (couverture par G8, nombre de tests, volume des propriétés). Pour une mesure de performance, refaire la mesure si un harnais existe, sinon la marquer « non vérifiée ».
4. **Tests qui mordent** : pour chaque changement de comportement important, introduire au moins une **mutation ciblée** dans le code de la PR (inverser une condition, décaler un indice, supprimer une vérification) et constater qu'un test échoue. La mutation est annulée ensuite. Une mutation qui survit est un constat.

### 3.3 Grille d'examen critique

| Axe | Questions |
|---|---|
| Justesse | Les invariants sont-ils établis et maintenus ? Les cas limites de [TESTING.md §4.2](../TESTING.md#42-cas-limites-obligatoires) sont-ils traités et testés ? Un débordement entier est-il possible (calculs en 32 bits, absence de `ckd_*`, produit de deux `ng_dim`) ? |
| Tests | Échoueraient-ils si le code était faux ? Chaque chemin d'erreur, dont l'échec d'allocation, est-il testé ? Les tests sont-ils déterministes ? Un test a-t-il été affaibli, désactivé ou supprimé ([AGENTS.md §3](../../AGENTS.md#3-règles-dor), règle 2) ? |
| Mémoire et erreurs | Propriété de chaque pointeur documentée ? Libération sur tous les chemins ? Allocation dans une boucle chaude ou dans `solve()` ? Les erreurs remontent-elles en `ng_status` sans `exit`, `abort` ni `printf` dans la bibliothèque ? |
| Architecture | Les couches de [ARCHITECTURE.md §3](../ARCHITECTURE.md#3-modules-et-dépendances) et les ADR sont-ils respectés ? Un ADR est-il contredit sans nouvel ADR ? Une dépendance externe est-elle ajoutée ? |
| API publique | Le changement est-il signalé dans la PR ? Doxygen complet (`@ownership`, erreurs, complexité) ? L'API est-elle cohérente avec l'existant et facile à mal utiliser ? |
| Normes | [CODING_STANDARDS.md](../CODING_STANDARDS.md) : nommage, types sémantiques, fonctions courtes, pas de nombre magique ni de construction interdite. |
| Documentation | Les documents concernés et `CHANGELOG.md` suivent-ils le code ? La description de la PR est-elle exacte ? |
| Périmètre | La PR fait-elle une seule chose ? Contient-elle du refactoring opportuniste ou des changements hors étape ? |
| Entrées non fiables | Tout ce qui vient d'un fichier, de la ligne de commande ou de l'appelant est-il validé ? |

### 3.4 Classer les constats

| Gravité | Définition | Effet |
|---|---|---|
| **BLOQUANT** | Bug démontré, règle d'or d'AGENTS.md enfreinte, test faux ou affaibli, porte rouge, critère de sortie de l'étape non tenu alors que la PR le déclare atteint, affirmation importante de la PR démentie. | empêche l'approbation |
| **MAJEUR** | Défaut qui doit être corrigé avant la fusion sans être un bug démontré : cas limite important non testé, mutation qui survit, documentation fausse, dette notable non signalée, choix de conception nettement contestable. | empêche l'approbation |
| **MINEUR** | Amélioration souhaitable : lisibilité, nom, commentaire, test supplémentaire utile. | n'empêche pas l'approbation |
| **QUESTION** | Point à clarifier par l'auteur, faute de preuve suffisante. | n'empêche pas l'approbation, sauf si la réponse révèle un défaut |

### 3.5 Rendre le verdict

| Verdict | Condition |
|---|---|
| **APPROUVÉE** | Aucun constat bloquant ni majeur, et toutes les vérifications du §3.2 ont été faites (ou leur impossibilité est expliquée et sans conséquence sur le verdict). |
| **CHANGEMENTS DEMANDÉS** | Au moins un constat bloquant ou majeur. |
| **DÉCISION HUMAINE REQUISE** | La PR soulève une question qui n'appartient pas au relecteur : conflit avec un ADR ou une demande humaine, spécification ambiguë, arbitrage de priorités ([AGENTS.md §6](../../AGENTS.md#6-quand-sarrêter-et-demander)). |

Le relecteur n'approuve jamais une PR dont il n'a pas pu vérifier les faits essentiels : il rend alors **CHANGEMENTS DEMANDÉS** ou **DÉCISION HUMAINE REQUISE**, en disant ce qui l'en a empêché.

## 4. Format du rapport

Le rapport est rédigé en français ([ADR-0008](../adr/0008-langues.md)), avec ce gabarit :

```markdown
## Revue indépendante — PR #<n> : <titre>

**Verdict : APPROUVÉE | CHANGEMENTS DEMANDÉS | DÉCISION HUMAINE REQUISE**
SHA relu : <sha> · Base : <branche> · Relecteur : <agent ou personne>

### Synthèse
<3 à 6 phrases : ce que fait la PR, sa qualité d'ensemble, ce qui motive le verdict>

### Vérifications effectuées
| Vérification | Résultat |
|---|---|
| CI sur le SHA relu | verte / rouge / absente |
| `check.sh --only G7,G8,G9` (ou autres portes) | OK / KO (détail) |
| Affirmations de la description recontrôlées | <liste : confirmée / démentie / non vérifiée> |
| Mutations ciblées | <n> essayées, <n> détectées (détail des survivantes) |

### Constats
| # | Gravité | Emplacement | Constat et preuve | Correction proposée |
|---|---|---|---|---|
| 1 | BLOQUANT | `src/x.c:42` | ... | ... |

### Points forts
<ce qui est bien fait et doit être conservé>

### Non vérifié
<ce qui n'a pas pu être contrôlé, et pourquoi>
```

## 5. Publication et suite

- Le rapport est publié **en commentaire de revue sur la PR**. GitHub interdit d'approuver une PR avec le compte qui l'a ouverte : quand le relecteur utilise ce compte, le verdict figure dans le texte du commentaire et la revue est de type « commentaire ».
- L'auteur répond à **chaque** constat bloquant, majeur ou question : corrigé (avec le commit), ou justifié (pourquoi le constat ne s'applique pas). Il ne marque pas lui-même un constat comme résolu.
- Après des corrections, une nouvelle revue indépendante porte sur le nouveau SHA. Elle peut se limiter aux corrections et aux constats ouverts, sauf si les corrections sont étendues.
- Pour une pile de PR, chaque PR est relue séparément ; une correction faite dans une PR antérieure impose de rebaser les suivantes, et leur revue porte alors sur le nouveau SHA.

## 6. Interdits

- Approuver sans avoir exécuté les vérifications du §3.2, ou en s'appuyant sur la seule description de la PR.
- Modifier la PR, pousser un commit ou réécrire l'historique.
- Relire une PR dont on est l'auteur ou à laquelle on a contribué.
- Atténuer un constat pour obtenir un verdict, ou en gonfler un sans preuve.
- Exiger un changement hors du périmètre de la PR : le noter comme suite possible, sans effet sur le verdict.

## 7. Agent relecteur Claude

Pour Claude Code, la définition [.claude/agents/pr-reviewer.md](../../.claude/agents/pr-reviewer.md) applique cette politique. On la lance sur une PR, dans un worktree isolé, sans lui transmettre le contexte de l'auteur.
