# Comprendre le dashboard du robot

Le bouton **Synthétiser le contrôleur** calcule une stratégie à partir de la grille, des commandes disponibles, des perturbations et de la mission. Le bouton **Simulation** applique ensuite cette stratégie à une pose de départ `(x1, x2, x3)`. Les deux premières coordonnées donnent la position et la troisième le cap, en radians.

## Qu’est-ce qu’une arête ?

Une arête est une transition possible entre deux cellules sous une commande donnée. Par exemple, si la commande 2 peut conduire de la cellule 17 aux cellules 18 et 19, on compte deux arêtes : `17 — commande 2 → 18` et `17 — commande 2 → 19`. La cellule de départ représente plusieurs poses et la perturbation peut changer le résultat.

Le compteur du dashboard mesure les arêtes **virtuelles** de l’abstraction du robot. Le code compte les successeurs des paires cellule–commande dont la boîte d’arrivée reste strictement dans le domaine spatial. Il réutilise des transitions relatives au lieu de stocker ce graphe complet. Le compteur ne représente donc ni les arêtes du produit avec l’automate, ni les commandes retenues dans le contrôleur, ni le trajet dessiné.

## Que signifie le domaine gagnant ?

Le domaine gagnant initial, noté `Q0`, est l’ensemble des cellules depuis lesquelles le contrôleur peut garantir la mission **pour tous les successeurs du modèle symbolique**. Le calcul tient compte du label de départ. Démarrer dans R1 peut donc valider immédiatement la première étape de la mission.

L’affichage `|Q0| : 120 / 4800`, par exemple, désignerait 120 cellules gagnantes parmi 4 800 cellules internes. Les cellules incluent le cap. Deux poses au même endroit peuvent obtenir des résultats différents si le robot regarde dans des directions différentes. Un domaine vide signifie que le modèle, la grille et la mission choisis ne donnent aucune cellule initiale gagnante. Une grille trop grossière peut aussi faire disparaître des cellules cibles.

## Que signifie le nombre de pas ?

Un pas est l’application d’une commande suivie d’un calcul du prochain état. Avec `tau = 1 s`, une réussite en 8 pas correspond à 8 secondes de temps simulé. La pose initiale correspond au pas 0. Le curseur **Pas t** affiche une observation enregistrée de la trajectoire. Le temps de synthèse affiché en secondes mesure le calcul sur l’ordinateur.

L’horizon borne le nombre de commandes de la simulation. Le dashboard utilise 200 pas par défaut. Le code actuel vérifie les observations de 0 à `horizon - 1`, ce qui peut faire manquer une arrivée au dernier pas.

## Que signifie Seed ?

La seed est la graine du générateur pseudo-aléatoire qui tire les perturbations à chaque pas. Sur la même version du programme et avec les mêmes paramètres, elle permet de rejouer une simulation. Pour trois trajectoires avec la seed 42, le dashboard utilise 42, 43 et 44. Changer la seed change les perturbations simulées. Le domaine gagnant est calculé à partir de leurs bornes et ne dépend pas de cette graine.

## Messages et erreurs

| Message ou famille de messages | Signification |
|---|---|
| `Mission reussie en N pas.` | L’automate a atteint un état final à l’observation N. |
| `Controleur non initialise.` ou `Synthetisez d'abord un controleur avant de simuler.` | Il manque un résultat de synthèse. Le bouton Simulation est normalement désactivé dans ce cas. |
| `Etat initial hors du domaine.` | La position initiale sort des limites de la grille. Le cap est traité comme périodique. |
| `Etat initial hors du domaine gagnant Q0.` | La cellule existe, mais le contrôleur calculé ne garantit pas la mission à partir de cette cellule. |
| `Sortie du domaine a t=N` | Un état atteint pendant la simulation ne se trouve plus dans la grille. |
| `Horizon maximal atteint (N pas).` | Le programme n’a observé aucune réussite avant la fin de sa boucle. |
| `N / M trajectoires reussies.` | Résumé d’un lot. L’échec détaillé de chaque trajectoire reste dans les résultats de simulation. |
| `Parametres modifies : relancez la synthese.` | Le formulaire a changé depuis la synthèse. Le résultat précédent correspond à l’ancienne configuration. |
| `Aucune region definie.`, `Une region a un nom vide.`, `Nom de region en double`, `Coordonnees invalides pour la region` | La définition des régions est invalide. Les limites doivent être finies et croissantes et les noms doivent être uniques après suppression des espaces extérieurs. |
| `Visit first: region inconnue`, `Then go to: region inconnue`, `Avoid: region inconnue` | Le champ référence un nom absent du tableau des régions. |
| `Le champ 'Visit first' doit contenir au moins une region.` | Il manque la première destination. |
| `Une meme region apparait dans deux categories.`, `Une region de 'Then go to' ne peut pas etre dans 'Avoid'.` | Une région est utilisée dans des rôles incompatibles. |
| `invalid grid width/count`, `reversed box`, `nonfinite coordinate`, `grid shape mismatch`, `vector dimension mismatch` | Taille de grille, bornes, valeurs ou dimensions invalides. Une borne de perturbation négative peut créer une boîte inversée. |
| `linspace: count must be positive`, `UnicycleDynamics: tau must be strictly positive and finite` | Il faut au moins une valeur par axe de commande et une période d’échantillonnage positive et finie. |
| `UnicycleDynamics::step: invalid vector dimensions`, `UnicycleDynamics::absoluteJacobianBounds: invalid input size`, erreurs de matrice/dérivées | Les données envoyées à la dynamique ou au calcul des enclosures ont une taille ou des valeurs incompatibles. Ces messages visent surtout les appels C++. |
| `automaton has no states/labels`, `automaton rows have different sizes`, `… automaton state out of range` | La table de l’automate est invalide. Le compilateur de mission construit normalement une table valide à partir des champs du dashboard. |
| `labels do not cover all system states`, `system label out of range`, `labelled system state out of range` | Des cellules ont des labels absents ou non reconnus par l’automate. |
| `controller inputs do not match final states`, `controller input out of range`, `controller inputs must be sorted and unique`, `current system state out of range`, `policy memory out of range`, `goal already reached`, `no controller entry for the current state and memory` | La politique, sa mémoire ou la demande de commande ne sont pas cohérentes. Ces exceptions peuvent remonter au CLI. Le dashboard actuel attrape les exceptions de synthèse et d’export, mais celles de simulation peuvent interrompre l’application. |
| `… overflow`, `std::bad_alloc` ou erreur de taille | Un calcul dépasse la plage d’un entier ou la mémoire disponible. La grille et l’automate déterminent ensemble la taille du calcul. |
| `Impossible d'ouvrir le fichier CSV : …` | Le chemin n’est pas accessible à l’écriture. Le dashboard exporte dans `simulation_dashboard.csv`, dans le dossier depuis lequel il a été lancé. |
| `GLFW Error …`, `Impossible d'initialiser GLFW.`, `Impossible de creer la fenetre OpenGL.` | La fenêtre ne peut pas démarrer. Le détail vient du système graphique. |
| `Echec de la synthese.`, `Erreur : …`, `ECHEC : …` | Préfixes génériques. Il faut lire le détail qui suit ou le message d’état de la trajectoire. |

Les erreurs de construction du graphe explicite et du produit explicite, comme `state/input pair budget exceeded` ou `product successor budget exceeded`, appartiennent aux API générales. Le robot utilise un produit implicite. Les messages CMake, du compilateur et de l’éditeur de liens arrivent avant le démarrage, tandis que les erreurs système et bibliothèques peuvent avoir un texte propre à la machine.
