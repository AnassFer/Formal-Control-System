# Audit du simulateur et du dashboard du robot

Version examinée : `15a91de`, branche `main`. Environnement : macOS arm64, Apple Clang et build CMake Release. Le [guide du robot](guide-robot.md) explique les paramètres et les messages affichés.

**Bilan : le scénario Papier fonctionne sur les 150 trajectoires essayées. Trois défauts peuvent cependant fausser le verdict d’une mission : la visite d’une région exclusive peut passer inaperçue, une grille de cap personnalisée peut contredire la dynamique, et l’arrivée au dernier pas est ignorée.** Le mode rapide du dashboard et le CLI par défaut donnent tous deux un domaine gagnant vide.

## Essais réalisés

| Essai | Résultat observé |
|---|---|
| Compilation locale | `robot_dashboard`, `robot_cli` et `symctrl_cli` compilés. Les avertissements ImGui/ImPlot et la bibliothèque dupliquée n’ont pas empêché la compilation. |
| Démarrage graphique | La fenêtre macOS s’est ouverte et a affiché le formulaire et la carte du preset rapide. L’outil a ensuite perdu l’accès à la fenêtre. Les interactions de synthèse, simulation et export ont été vérifiées par les méthodes C++ de `DashboardState` et du moteur. Les clics, le déplacement sur la carte et l’animation n’ont pas été validés de bout en bout. |
| CLI robot sans argument | 1 551 360 arêtes virtuelles, `Q0 = 0 / 4800`, deux échecs pour la figure 6, `0 / 50` pour la figure 7, aucun CSV et code de sortie 0. |
| Préréglage **Test rapide** avec régions élargies | 1 551 360 arêtes, `Q0 = 0 / 4800`. Le départ prérempli est rejeté. |
| Préréglage **Papier** | 197 384 512 arêtes virtuelles, `Q0 = 216 680 / 300 000`, synthèse d’environ 1 s dans l’essai local. À partir de `(0.5, 4.5, 0)` avec seed 42, réussite en 18 pas. |
| Rejeu de la seed et export normal | Deux exécutions avec seed 42 ont donné les mêmes poses, commandes et mémoires. La seed 43 a changé la première pose d’arrivée. Le CSV normal contient les colonnes `t,x1,x2,x3,u1,u2,mem` et 19 observations de `t=0` à `t=18`. |
| Entrées invalides | Douze essais avec bornes de grille, nombre de cellules ou de commandes, perturbation, période, régions et mission invalides ont produit une erreur. Un départ à `(-1,4.5,0)` a été signalé hors domaine et `(5,5,0)` hors domaine gagnant. |
| Comparaison rapide / explicite | Même ensemble gagnant sur quatre configurations, dont deux missions faisables. Sur la cible simple `[1,3] × [0,10]` avec la grille `20×20×12`, les deux solveurs donnent 1 028 cellules gagnantes. Les commandes rapides vérifiées diminuent les rangs de référence. |
| CLI des autres modèles | `--help`, `thermostat`, `thermostat-policy`, `building-n3` et `export-thermostat` passent. L’export contient les quatre fichiers attendus et une commande inconnue donne le code 1. |

Les 150 trajectoires Papier utilisent les graines 100 à 149 pour chacun des trois départs. Les positions des points enregistrés ont été comparées directement aux rectangles. Dans cet échantillon, chaque trajectoire visite une seule des deux régions R1/R2, évite R4 aux observations et termine dans R3.

| Départ | Réussites | Région visitée | Pas observés |
|---|---:|---|---:|
| `(0.5, 4.5, 0)` | 50/50 | R1 | 17–19 |
| `(0.5, 4.0, 0)` | 50/50 | R2 | 17–19 |
| `(6.8, 9.0, 0)` | 50/50 | R2 | 15–16 |

## Défauts reproduits

Les priorités P1 couvrent les verdicts incorrects et les arrêts possibles. P2 couvre les erreurs d’interface, les exports et la démo.

| Priorité | Défaut et reproduction | Correction proposée |
|---|---|---|
| P1 | **La règle XOR peut être violée avec des zones disjointes.** Grille `20×20×48` sur le domaine par défaut, seule commande `(1,0)`, perturbations nulles, A = `[1,2] × [0,10]`, B = `[2.49,2.51] × [4.4,4.6]`, cible G = `[3,10] × [0,10]`. Depuis `(1.5,4.5,0)`, le robot est dans A au départ et entre dans B à `t=1` en `(2.5,4.5)`. B ne contient pas la cellule entière. Le label reste neutre et le programme annonce une réussite à `t=2`. [Calcul des labels](../app/mission_compiler.cpp#L188) | Garder l’inclusion pour valider une visite et traiter une intersection avec une région interdite par la mémoire comme une issue à exclure. Une représentation des labels possibles permet de traiter aussi les cellules ambiguës. |
| P1 | **Le cap n’a pas la même période dans la grille et la dynamique.** Avec `x3 ∈ [0,1]`, `nx3=10`, cible `[1,2] × [0,10]`, commande unique `(1,0)` et perturbations nulles, les départs `(0.7,4.5,0.35)` et `(0.7,4.5,3.35)` sont associés à la même cellule gagnante. Le premier réussit en un pas et le second sort du domaine. La grille utilise la largeur de l’intervalle comme période. La dynamique utilise `2π`. [Configuration de grille](../app/robot_simulation.cpp#L355) · [Dynamique](../app/unicycle_dynamics.cpp#L8) | Garder une période de `2π` et partager la normalisation du cap entre la simulation et la grille. Refuser les intervalles personnalisés incompatibles. |
| P1 | **Une arrivée à l’horizon est perdue.** Preset Papier, départ `(0.5,4.5,0)`, seed 42. Avec un horizon de 18, le programme annonce un échec et le CSV potentiel s’arrête à `t=17`. Avec 19, il annonce une réussite en 18 pas. L’API avec un horizon de 0 rate aussi un départ déjà accepté. [Boucle](../app/robot_simulation.cpp#L438) | Observer la pose initiale, puis chaque pose après une commande, y compris celle qui suit la dernière commande autorisée. Conserver le dernier point en cas de réussite, de sortie ou de limite atteinte. |
| P1 | **Une entrée non finie peut terminer l’application.** Le parseur ImGui a accepté `1e999` et produit l’infini dans un test isolé. Injecter ensuite une pose initiale non finie au niveau de `DashboardState::runSimulations` provoque une exception `nonfinite coordinate` non interceptée. Le chemin de saisie n’a pas été rejoué par clic dans la fenêtre. [Simulation du dashboard](../app/dashboard/dashboard_state.cpp#L143) | Valider les valeurs des formulaires avant simulation et intercepter les exceptions dans le callback de simulation pour afficher une erreur. |
| P1 | **Des valeurs finies provoquent des conversions hors plage.** Avec `w3Max=1e20`, UBSan détecte une conversion de `-1.90986e20` en entier signé dans le solveur rapide. Un test de grille indépendant détecte le même type d’erreur pour une boîte ponctuelle à `1e300` sur une grille `[0,1]`. Une normalisation périodique avec des valeurs finies extrêmes produit également une conversion de NaN. [Solveur rapide](../app/robot_simulation.cpp#L115) · [Grille](../src/discretization/uniform_grid.cpp#L207) | Détecter les intervalles couvrant tout le cercle, les boîtes disjointes et les valeurs hors plage avant toute conversion entière. Vérifier les produits de dimensions et les budgets mémoire avant d’allouer. |
| P1 | **Le calcul de la seed déborde.** Seed `2147483647`, deux trajectoires. L’addition signée `randomSeed + i` déborde avant sa conversion en entier non signé. UBSan le confirme. [Seed](../app/dashboard/dashboard_state.cpp#L161) | Définir une plage de saisie et additionner dans le type non signé choisi, avec une règle explicite à sa borne. |
| P1 | **Le dashboard peut appliquer un contrôleur périmé.** Lancer la synthèse avec `tau=1`, modifier le formulaire à `tau=2` avant la récupération du résultat. Le résultat correspond à `tau=1`, mais `configDirty` devient faux. Après une synthèse en erreur avec `tau=0`, l’ancienne politique est encore disponible et lancer Simulation efface l’erreur. Le tracé se sert des régions en cours d’édition. [Récupération du résultat](../app/dashboard/dashboard_state.cpp#L106) · [Activation du bouton](../app/dashboard/dashboard_ui.cpp#L352) | Associer un numéro de version à chaque configuration. Désactiver la simulation si le contrôleur est périmé, ou afficher explicitement le snapshot actif et tracer ses régions. |
| P1 | **Le bord d’un obstacle fermé peut être ignoré.** Obstacle `[0,1] × [0,1]`, cible `[1,2] × [0,2]`, départ `(1,0.5,0)`. Le programme peut annoncer une réussite immédiate. Le test d’intersection exclut le contact de frontière. Ce verdict viole la convention de rectangles fermés. [Intersection](../app/mission_compiler.cpp#L55) | Définir explicitement la règle au bord et tester le contact. Pour une région fermée à éviter, conserver les cellules qui touchent sa frontière. |
| P2 | **L’ordre des régions peut modifier la mission.** Avec une cible et une région inutilisée de rectangles identiques `[1,2] × [0,2]`, leur permutation seule fait passer `Q0` de 0 à 96. Le premier label retenu masque l’autre. Deux régions XOR identiques sont aussi acceptées et le programme peut déclarer une réussite. [Priorité des labels](../app/mission_compiler.cpp#L205) | Vérifier les chevauchements incompatibles. Ne pas laisser une région hors mission masquer une région de la mission. |
| P2 | **Le mode rapide ne fournit pas de démo gagnante.** Le résultat vide est cohérent avec le solveur explicite pour les configurations vérifiées. Le problème est le preset proposé et son affichage « Contrôleur prêt ». Le CLI reprend la petite grille avec les régions exactes et sort avec 0 même lorsque toutes les exécutions échouent. [Preset du dashboard](../app/dashboard/dashboard_state.cpp#L17) · [CLI](../app/robot_cli.cpp#L10) | Afficher « Aucune cellule gagnante » et désactiver Simulation si `Q0` est vide. Une grille de `60×60×30` avec régions exactes a été essayée : 77 344 cellules gagnantes et 20 pas pour le départ par défaut avec seed 42. Proposer au CLI un preset Papier et un statut de sortie exploitable. |
| P2 | **L’export peut annoncer une écriture qui a échoué.** Une limite d’écriture de zéro octet permet d’ouvrir le fichier, puis bloque l’écriture. Le fichier reste vide et le dashboard annonce la réussite. Une tentative d’ouverture vers un dossier invalide a bien produit une erreur. Une trajectoire sans point produit aussi un CSV avec seulement l’en-tête. [Export](../app/robot_simulation.cpp#L473) | Contrôler les erreurs après écriture et fermeture. Refuser ou signaler les exports sans données. Utiliser un chemin choisi et confirmer un écrasement. |
| P2 | **Les paramètres incohérents ne sont pas toujours signalés.** `u1Min=1, u1Max=0.25, nu1=3` crée la liste `1, 0.625, 0.25`. Une cible `[20,21] × [20,21]` sur la carte par défaut est acceptée et donne `Q0=0`. [Commandes](../app/robot_simulation.cpp#L20) | Indiquer les bornes inversées, les régions hors carte et les régions de mission auxquelles aucune cellule n’est attribuée. |

## Autres comportements relevés dans le code

- Après plusieurs simulations, le résumé donne le nombre de réussites, mais l’interface ne permet pas de consulter le motif d’échec de chaque essai. Deux départs hors domaine ont par exemple donné uniquement `0 / 2 trajectoires reussies.` dans le résumé. Le graphique superpose les trajectoires et `selectedTrajectory` reste à 0. L’animation et l’export ciblent donc la première. Afficher une liste donnant seed, état final, nombre de pas et message de chaque trajet. [Résumé](../app/dashboard/dashboard_state.cpp#L179) · [Barre de simulation](../app/dashboard/dashboard_ui.cpp#L335)
- Le bouton d’ajout utilise le nombre courant de régions pour former le nom. Après suppression de R1 en gardant R2, R3 et R4, il construit un nouveau R4. Renommer une région référencée laisse aussi le nom d’origine dans la mission. Générer un identifiant stable et proposer des listes déroulantes pour les régions. [Édition des régions](../app/dashboard/dashboard_ui.cpp#L257)
- Le lot de simulations tourne sur le fil de la fenêtre et n’a pas de plafond pour `Nb traj.`. La fermeture attend le calcul de synthèse sans possibilité de l’annuler. Le compteur de progression saute de `3/5` à `5/5`. Exécuter les lots en arrière-plan, ajouter une annulation et donner la progression de la partie coûteuse. Aucun essai de saturation mémoire ou de blocage par gros lot n’a été provoqué. [État du dashboard](../app/dashboard/dashboard_state.cpp#L11) · [Progression](../app/robot_simulation.cpp#L354)
- Le dashboard impose un horizon de 200 pas dans le parcours normal et exporte toujours vers `simulation_dashboard.csv`. La dernière ligne d’un CSV réussi contient `u1=u2=0` pour marquer l’absence de nouvelle commande. Ajouter un champ d’horizon, préciser la convention terminale et utiliser un nom incluant la seed ou la date. [État](../app/dashboard/dashboard_state.hpp#L35) · [Export et animation](../app/dashboard/dashboard_ui.cpp#L364)
- L’évitement vérifie les poses échantillonnées. Dans un essai sans perturbation, un pas direct de `(0.75,4.5)` à `(2.75,4.5)` réussit alors que le segment affiché traverse l’obstacle `[1.1,1.2] × [4.4,4.6]`. C’est une limite du modèle discret. Si la mission exige aussi l’évitement entre deux observations, contrôler la zone parcourue pendant le pas. [Dynamique](../app/unicycle_dynamics.cpp#L26)

## Tests à intégrer en priorité

1. Transformer les contre-exemples XOR, cap, bord d’obstacle et horizon en tests automatiques avec résultats attendus.
2. Comparer le solveur rapide au produit explicite sur de petites grilles variées. Vérifier à la fois les ensembles gagnants et la baisse du rang pour chaque successeur de la commande retenue.
3. Tester les invariants : décaler un cap de `2π` doit conserver la décision, permuter une région inutilisée doit conserver la mission, et augmenter l’horizon ne doit pas annuler une réussite déjà observée.
4. Ajouter des tests du dashboard pour les changements de configuration pendant une synthèse et des tests d’export avec un échec après ouverture. Activer les détecteurs de dépassement signé et de conversion flottante dans les builds de vérification.
5. Ajouter un scénario de démonstration court et un lancement de validation du preset Papier. Donner le nombre de cellules correctement labellisées par zone, le rang initial, le motif d’un départ perdant et les résultats par seed.

## Rejouer

Sur macOS, depuis la racine du projet :

```sh
cmake -S . -B build-macos -DCMAKE_BUILD_TYPE=Release
cmake --build build-macos --target robot_cli robot_dashboard symctrl_cli --parallel 2
./build-macos/robot_cli
./build-macos/robot_dashboard
```

Dans le dashboard, comparer **Test rapide** et **Papier**, en relançant la synthèse après chaque sélection. Pour le preset Papier, la pose initiale est `(0.5,4.5,0)` et la seed par défaut est `42` au démarrage. Le test de l’horizon utilise l’argument `maxSteps` de l’API `simulateClosedLoop`.

Les tests C++ ont utilisé les bibliothèques locales et des fichiers temporaires. Les mesures sont locales à ce Mac. Les enclosures à virgule flottante restent déclarées `UncertifiedFloat`. Windows, Linux, une exécution matérielle et une validation continue de l’évitement restent hors de ces essais.
