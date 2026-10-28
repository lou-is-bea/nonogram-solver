# Politique de revue des pull requests

> Dernière revue : 2026-10-28. Ce document est **normatif**. Il complète [AGENTS.md](../../AGENTS.md) et [CONTRIBUTING.md §3](../../CONTRIBUTING.md#3-cycle-de-vie-dune-modification) : toute PR reçoit, avant la revue humaine, une **revue indépendante** menée selon cette politique. Il s'adresse au relecteur (une personne ou un agent IA) et à l'auteur qui lui répond.

---

## 1. Principes

1. **Indépendance.** Le relecteur n'est pas l'auteur. Un agent IA relecteur démarre sans contexte : il n'a accès ni à la conversation ni aux notes de l'auteur, seulement au dépôt, à la PR et à la CI. Il ne relit jamais une PR à laquelle il a contribué.
2. **Scepticisme.** La description de la PR est une liste d'**affirmations à vérifier**, pas une preuve. « Vérifié », « couverture de 97 % », « médiane de 0,23 ms », « aucun test affaibli » : chaque affirmation qui compte pour le verdict est recontrôlée, ou signalée comme non vérifiée. Seule exception : un accord humain relayé selon le §5.1, dont le relecteur vérifie le périmètre mais pas l'authenticité.
3. **Regard critique.** Le relecteur cherche ce qui manque autant que ce qui est faux : cas limites non testés, tests qui passeraient avec un code faux, chemins d'erreur oubliés, documentation en retard, choix discutables. Il conteste un choix de conception s'il le juge mauvais, même s'il est cohérent avec le reste de la PR, en le classant à sa juste gravité.
4. **Preuves.** Chaque constat cite un fichier et une ligne, une commande et sa sortie, ou un scénario concret qui produit un résultat faux. Un soupçon sans preuve est une **question**, pas un défaut.
5. **Lecture seule.** Le relecteur ne pousse aucun commit et ne modifie pas la PR. Il propose ; l'auteur corrige.
6. **Décision humaine.** Un verdict APPROUVÉE est nécessaire à la fusion, mais il ne suffit pas : la fusion reste approuvée par un responsable du projet ([CONTRIBUTING.md §3](../../CONTRIBUTING.md#3-cycle-de-vie-dune-modification)). Cet accord peut être donné dans la session de travail et relayé dans la PR par l'agent auteur (§5.1). Aucune décision humaine ne remplace le verdict ; une décision humaine justifiée peut en revanche orienter le verdict (§3.5).

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

1. **CI** : l'exécution de la CI sur le SHA de tête est verte (`gh pr checks <n>`). Une CI rouge ou absente est un constat bloquant, sauf justification écrite dans la PR. Si elle tourne encore, le relecteur attend sa fin ; s'il ne peut pas l'attendre, il la marque « non vérifiée » et ne rend pas APPROUVÉE.
2. **Portes en local** : relancer `./scripts/check.sh` complet depuis un build propre, même si la tâche CI `full-check` exécute déjà G0 à G9 dans le conteneur de référence ([TESTING.md §5](../TESTING.md#5-intégration-continue)). Cette relance montre que la PR passe aussi hors de la CI et fournit les chiffres de couverture à recalculer (G8). Une porte qui ne peut pas tourner est signalée, jamais présentée comme réussie. Relancer aussi toute porte dont le résultat est contesté ou paraît improbable.
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

Une **décision humaine justifiée** peut orienter le verdict, jamais le remplacer. Après un verdict DÉCISION HUMAINE REQUISE, ou sur un constat que l'auteur conteste, le responsable du projet tranche dans la PR (directement, ou en session selon le §5.1) et motive sa décision. Il peut par exemple lever une question, choisir entre deux conceptions, ou accepter une dette signalée et en fixer la suite. Le relecteur rend alors un nouveau verdict qui tient compte de cette décision. La décision ne dispense d'aucune vérification du §3.2 et ne rend jamais acceptable un constat BLOQUANT (§3.4). Seul un accord humain prévu par une règle d'or elle-même, comme celui de la règle 2, peut lever le constat qui relève de cette règle.

Quand une PR a un constat bloquant ou majeur et soulève aussi une question qui revient à un humain, le verdict est **CHANGEMENTS DEMANDÉS** et la question figure dans le rapport : les corrections viennent d'abord.

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
| `./scripts/check.sh` complet (local) | OK / KO (détail, portes SKIP) |
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
- Pour une pile de PR, chaque PR est relue séparément ; une correction faite dans une PR antérieure impose de rebaser les suivantes, et le §5.2 dit si leur verdict est reporté sur le nouveau SHA ou si une nouvelle revue est nécessaire.

### 5.1 Accord humain donné dans la session de travail

Le responsable humain du projet peut donner son accord à l'agent auteur directement dans la session de travail, sans l'écrire lui-même dans la PR. C'est le cas, entre autres, pour la fusion d'une PR, l'approbation de sorties de référence ou un accord exigé par une règle d'or d'AGENTS.md. Cet accord vaut accord humain tracé, à quatre conditions :

1. **Trace dans la PR.** L'agent auteur publie dans la PR un commentaire qui dit qu'il relaie un accord donné en session. Le commentaire cite les mots du responsable **mot pour mot** et donne la date.
2. **Périmètre explicite.** Le commentaire nomme ce qui est accepté (les décisions une par une, ou les cas concernés pour des sorties de référence) et le **SHA de tête** auquel l'accord a été donné. Ce qu'un commit ultérieur fait à l'accord est fixé par le §5.2.
3. **Rien d'ajouté.** L'agent ne reformule pas l'accord pour l'élargir. Ce que le responsable n'a pas dit n'est pas accordé, et toute ambiguïté se règle par une question dans la session.
4. **Mention à la fusion.** Le message de *squash merge* rappelle que l'accord a été donné dans la session de travail. L'auteur rédige ce message et vérifie la mention **avant** de lancer la fusion.

GitHub n'enregistre pas d'approbation venant du compte qui a ouvert la PR. Quand ce compte est partagé avec l'agent, la protection de `main` ne peut donc pas être satisfaite par une approbation : la fusion passe par le contournement administrateur (`gh pr merge --squash --admin`). Comme ce contournement ignore toutes les protections de `main`, il n'est permis que si quatre conditions sont réunies, qui reprennent ces protections :

- le responsable a demandé la fusion, et sa demande est tracée selon les conditions 1 à 4 ;
- la branche est à jour avec `main` ; sinon l'auteur la rebase d'abord, et le §5.2 dit ce que deviennent le verdict et les accords ;
- la CI est verte sur le SHA fusionné, que l'auteur fixe avec `--match-head-commit <sha>` ;
- ce SHA a reçu un verdict APPROUVÉE, directement ou par report selon le §5.2. Aucun autre verdict ne permet la fusion.

Le relecteur tient une telle trace pour un accord humain valable. Il ne la conteste pas au motif qu'elle est publiée par l'agent ou avec un compte partagé. Il vérifie en revanche que le périmètre cité correspond au contenu de la PR, et il signale tout écart, par exemple un cas modifié après l'accord. Il signale aussi tout accord invoqué sans commentaire de trace.

### 5.2 Nouveaux commits et rebasages

Un verdict et un accord humain portent sur un SHA précis. Comme pour l'option de GitHub qui rejette les approbations périmées à chaque nouveau commit, tout changement du contenu de la PR les fait tomber. Un rebasage qui ne change pas ce contenu les conserve, à condition que l'auteur le prouve. Le **contenu de la PR** est son diff propre, `git diff <base>...<head>` (§2).

| Changement après le verdict ou l'accord | Verdict APPROUVÉE | Accord de fusion | Accord sur un périmètre (sorties de référence, décisions) |
|---|---|---|---|
| **Nouveau commit** (correction, ajout, retouche) | tombe : nouvelle revue sur le nouveau SHA, limitée au delta selon le §5 | tombe : nouvelle demande du responsable | tombe pour ce que le commit modifie, reste valable pour le reste |
| **Rebasage sans conflit ni changement du contenu** | reporté sur le nouveau SHA | reporté | reporté |
| **Rebasage avec résolution de conflit, ou qui change le contenu** | traité comme un nouveau commit : la revue porte sur les parties résolues ou changées | tombe | tombe pour ce qui a changé |

Pour reporter un verdict ou un accord après un rebasage, l'auteur publie dans la PR, avant la fusion :

1. l'ancien et le nouveau SHA ;
2. la preuve que le contenu est inchangé : `git range-diff <ancienne base>..<ancien head> <nouvelle base>..<nouveau head>`, où chaque commit est marqué `=` ; ou, si la base n'a pas changé (force-push du même contenu), l'égalité des arbres, `git rev-parse <ancien head>^{tree}` et `git rev-parse <nouveau head>^{tree}` donnant le même résultat ;
3. le résultat de la CI sur le nouveau SHA, qui doit être verte.

Sans cette preuve, le rebasage compte comme un nouveau commit. Avant la fusion, l'auteur s'assure que la preuve est publiée et complète, et le message de fusion y renvoie ; si une nouvelle revue a lieu, le relecteur la vérifie aussi. Un report n'est jamais implicite.

## 6. Interdits

- Approuver sans avoir exécuté les vérifications du §3.2, ou en s'appuyant sur la seule description de la PR.
- Modifier la PR, pousser un commit ou réécrire l'historique.
- Relire une PR dont on est l'auteur ou à laquelle on a contribué.
- Atténuer un constat pour obtenir un verdict, ou en gonfler un sans preuve.
- Exiger un changement hors du périmètre de la PR : le noter comme suite possible, sans effet sur le verdict.

## 7. Agent relecteur Claude

Pour Claude Code, la définition [.claude/agents/pr-reviewer.md](../../.claude/agents/pr-reviewer.md) applique cette politique. On la lance sur une PR, dans un worktree isolé, sans lui transmettre le contexte de l'auteur.
