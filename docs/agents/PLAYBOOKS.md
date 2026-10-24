# Procédures pour les agents

> Dernière revue : 2026-10-25. Ces procédures complètent [AGENTS.md](../../AGENTS.md). Chaque procédure se termine par `./scripts/check.sh` et par le rapport décrit dans [AGENTS.md §8](../../AGENTS.md#8-format-du-rapport-de-fin-de-tâche).

---

## P1 — Ajouter un solver

**Prérequis** : l'étape de la feuille de route correspondante est en cours, et la spécification `docs/solvers/<nom>.md`, rédigée à partir du [gabarit](../solvers/TEMPLATE.md), est **approuvée par un humain**. Si elle ne l'est pas, rédige-la, soumets-la et arrête-toi là.

1. Crée `src/solvers/s<N>_<nom>/` avec `<nom>.c` et un en-tête interne `<nom>.h` qui expose uniquement `extern const ng_solver_vtable ngi_solver_<nom>;`.
2. Implémente `workspace_size`, `init` et `solve` selon le contrat de [ARCHITECTURE.md §6](../ARCHITECTURE.md#6-interface-des-solvers) :
   - aucune allocation (l'allocateur compteur doit rester à 0 pendant `solve`) ;
   - aucune récursion (pile explicite dans l'espace de travail) ;
   - accès à la grille par copies de ligne (`ng_grid_load_line` / `ng_grid_store_line`), jamais par le stockage interne ;
   - les types sémantiques et `ckd_*` dans `workspace_size`, avec une formule de taille documentée dans la spécification et un test qui la vérifie ;
   - respect de `max_nodes`, `max_time_ms` et `max_memory_bytes`.
3. Ajoute **une ligne** dans `src/solvers/registry.c`.
4. Ajoute le fichier source à la cible `ng_solvers` dans `src/solvers/CMakeLists.txt`.
5. Tests Criterion, dans `tests/unit/test_solver_<nom>.c` et `tests/property/` (avec `.timeout` sur chaque test) :
   - les cas limites de [TESTING.md §4.2](../TESTING.md#42-cas-limites-obligatoires) ;
   - les propriétés P-S1 à P-S6 qui s'appliquent à ce solver (un solver incomplet est concerné par P-S2, pas par P-S1) ;
   - un test différentiel contre les solvers complets existants sur `tiny` ;
   - chaque résultat `SOLVED` passe par `ng_verify`.
6. Ajoute le solver au harnais de benchmark et consigne les mesures dans la PR.
7. Mets à jour : la spécification (avec les mesures réelles), le tableau de statut de [ROADMAP.md](../ROADMAP.md), `CHANGELOG.md` et la liste des solvers dans le README.

## P2 — Corriger un bug

1. Écris un test qui reproduit le bug et **constate qu'il échoue**. Si le bug vient d'une entrée, ajoute le fichier dans `tests/data/regressions/<ticket>-<description>.non`.
2. Identifie la cause racine et explique-la dans la PR : quel invariant était violé, et pourquoi les tests existants ne l'ont pas détecté.
3. Corrige le code avec le changement minimal.
4. Vérifie que le test passe et qu'aucun autre ne régresse.
5. Si une famille de bugs semblables est possible, propose (sans l'implémenter d'office) une propriété ou une assertion qui les détecterait.

## P3 — Ajouter ou modifier une fonction de l'API publique

1. Vérifie qu'aucun ADR ne s'y oppose. Si le changement est **incompatible**, arrête-toi et demande (voir [AGENTS.md §6](../../AGENTS.md#6-quand-sarrêter-et-demander)).
2. Écris la déclaration et son commentaire Doxygen complet (`@brief`, `@param`, `@return`, `@ownership`, `@complexity`) **avant** l'implémentation.
3. Ajoute les tests : nominal, arguments `nullptr`, bornes (y compris au-delà de 2³² cases avec l'allocateur plafonné), échecs d'allocation.
4. Implémente la fonction en respectant les normes, en particulier la validation des arguments, `[[nodiscard]]` et les types sémantiques de `types.h`.
5. Mets à jour [ARCHITECTURE.md §5](../ARCHITECTURE.md#5-api-publique-esquisse) si la forme générale de l'API change, et ajoute une entrée « Ajouté » ou « Modifié » dans `CHANGELOG.md`.

## P4 — Ajouter des puzzles au corpus

1. Indique la provenance et la licence dans `tests/data/README.md`. **Aucun fichier dont la licence est inconnue n'est accepté.**
2. Place le fichier dans la bonne catégorie (`tiny`, `small`, `multi`, etc.) et respecte la convention de nommage.
3. Pour un puzzle à solution unique, ajoute `solutions/<nom>.sol` et vérifie-la avec `nonogram verify`.
4. Pour un puzzle de benchmark (`bench/corpus/`), ne modifie **jamais** un fichier existant : ajoute-en un nouveau. Les références de performance en dépendent.

## P5 — Optimiser une fonction

1. Mesure la situation de départ avec le harnais de benchmark, et indique la machine et le commit.
2. Garde l'implémentation simple comme référence (`*_reference`), utilisée dans un test différentiel.
3. Implémente l'optimisation, puis mesure à nouveau.
4. Dans la PR, présente le tableau avant/après (temps médian et pic mémoire) sur tout le corpus concerné. Une optimisation qui fait gagner moins de 5 % au prix d'un code nettement plus complexe est en principe refusée.

## P6 — Créer un module

1. Vérifie que la place du module dans les couches est conforme à [ARCHITECTURE.md §3](../ARCHITECTURE.md#3-modules-et-dépendances), ou proposes-en une par un ADR.
2. Crée `src/<module>/CMakeLists.txt` avec une cible `ng_<module>`. N'utilise `target_link_libraries` que vers les couches autorisées.
3. Ajoute le module à la bibliothèque agrégée et à `check.sh` (couverture, clang-tidy).
4. Documente-le dans ARCHITECTURE.md (arborescence, tableau des modules, diagramme).
