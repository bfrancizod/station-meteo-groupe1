# Soutenance : compiler, téléverser et afficher les mesures

Fiche pour ton PC Linux actuel. Suivre les étapes dans l’ordre.
Les commandes utilisent le chemin complet de PlatformIO pour éviter
« pio : commande introuvable ».

## 1. Ouvrir le projet

Dans un terminal Linux :

```bash
cd ~/projets/station-meteo-groupe1
```

Garder ce terminal ouvert pour la suite.

## 2. Compiler le programme

```bash
~/.platformio/penv/bin/pio run -e nucleo_l476rg
```

Attendre **[SUCCESS]** avant de continuer. La compilation transforme le code C
en programme utilisable par le STM32. **La carte peut être débranchée à cette étape.**

## 3. Téléverser le programme sur la carte

Brancher la Nucleo sur son connecteur **ST-LINK** avec le câble USB.
Fermer le dashboard, `cat` et les moniteurs série avant le téléversement.

```bash
~/.platformio/penv/bin/pio run -e nucleo_l476rg -t upload
```

Attendre **[SUCCESS]**. Le programme est écrit dans la mémoire de la carte,
puis la station redémarre. Ne maintenir aucun bouton rouge ou vert au démarrage
pour partir en mode standard.

## 4. Ouvrir la console et tester les commandes

Dans le même terminal Linux :

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200 --echo
```

`--echo` permet de voir les caractères que tu tapes.
Un seul lecteur doit utiliser le port série : laisser le dashboard fermé.

**À partir de maintenant, taper les commandes suivantes dans le moniteur,
avec Entrée après chaque ligne :**

```text
STATUS
SHOW
```

- `STATUS` affiche le mode, les défauts, l’état de la RTC et l’option GPS de démonstration.
- `SHOW` affiche les paramètres réellement utilisés par la station.
- `HELP` permet de retrouver les autres commandes.

### Activer le GPS de démonstration pour une présentation en intérieur

Toujours dans le moniteur :

```text
GPS_DEMO=1
STATUS
```

Vérifier que `STATUS` indique `GPS_DEMO=1`. Cette option fonctionne aussi en maintenance.
Pendant une acquisition, après **15 secondes sans position GPS valide**, le
programme utilise `0,0 (DEMO)`. Les autres capteurs restent réels.
Le défaut GPS peut alors disparaître ; les autres défauts restent signalés.

**Après un téléversement, RESET ou débranchement, il faut renvoyer `GPS_DEMO=1`.**
Le nom de la commande est `GPS_DEMO`, pas `GPS_MODE`.

### Afficher les mesures détaillées dans le terminal

Depuis le mode **standard**, maintenir le **bouton rouge pendant 5 secondes**,
puis le relâcher. Attendre le message **MODE MAINTENANCE**.

Si la station est en mode économique, un premier appui rouge de 5 secondes
revient au standard ; un nouvel appui rouge de 5 secondes entre en maintenance.

En maintenance, les mesures s’affichent automatiquement : date, température,
humidité, pression, luminosité, latitude et longitude.
Avec le GPS de démonstration sans réception satellite, prévoir environ
**16 secondes entre deux acquisitions complètes**.

Exemple pour les coordonnées remplacées :

```text
Latitude     : 0.000000 (DEMO)
Longitude    : 0.000000 (DEMO)
```

Masquer puis découvrir le capteur de lumière entre deux acquisitions pour montrer
une variation. La LED est orange fixe en maintenance si aucun défaut n’est présent.
Taper `STATUS` pour vérifier l’état ; `ERR=0x00` signifie qu’aucun défaut n’est signalé.

## 5. Passer à l’interface graphique

1. Quitter le moniteur avec **Ctrl+C** pour libérer le port série.
2. Laisser la carte branchée et en **maintenance** : ne pas appuyer sur RESET.
3. Dans le terminal Linux, toujours à la racine du projet, lancer :

```bash
/usr/bin/python tools/station_dashboard.py
```

Dans la fenêtre :

1. Choisir **/dev/ttyACM0** dans la liste des ports ; cliquer sur **Actualiser** si nécessaire.
2. Cliquer sur **Connecter**.
3. Attendre une acquisition complète : les valeurs et les courbes apparaissent.
4. Utiliser **Exporter CSV** pour enregistrer sur le PC les mesures reçues pendant la session.

L’interface est créée par `tools/station_dashboard.py`. Elle reçoit les données de
la carte par USB série. La maintenance envoie les blocs détaillés nécessaires aux
courbes ; en standard et en économie, les mesures sont enregistrées sur SD et la
console affiche surtout les bilans.

## 6. Si quelque chose ne fonctionne pas

| Problème | Action |
|---|---|
| `pio : commande introuvable` | Utiliser le chemin complet `~/.platformio/penv/bin/pio` indiqué dans cette fiche. |
| `/dev/ttyACM0` absent ou incorrect | Vérifier le branchement ST-LINK et lister les ports avec la commande ci-dessous. Adapter le port dans le moniteur et le dashboard. |
| Port occupé ou affichage incomplet | Fermer les autres lecteurs du port : moniteur, `cat`, miniterm ou dashboard. |
| L’interface s’ouvre mais aucune mesure n’arrive | Vérifier la connexion et le mode maintenance. Attendre un cycle complet. |
| Le GPS affiche `NA` après un redémarrage | Revenir dans le moniteur et réactiver `GPS_DEMO=1`, puis reprendre l’étape 5. |
| La LED clignote encore avec GPS DEMO | Lire `STATUS` : un autre défaut peut être présent. |

Pour lister les ports, dans un terminal Linux :

```bash
~/.platformio/penv/bin/pio device list
```

Pour tester uniquement l’interface **sans carte**, avec des mesures entièrement
fictives, fermer l’interface précédente puis lancer :

```bash
/usr/bin/python tools/station_dashboard.py --demo
```

Ce mode simule toutes les données sur le PC. Il est différent de `GPS_DEMO=1`,
qui remplace uniquement la position GPS sur la carte.

## 7. Terminer la démonstration

En maintenance, attendre **« SD liberee : retrait autorise. »** avant de retirer
la SD ou de couper l’alimentation. Fermer le dashboard, puis débrancher la carte.

Pour l’installation des outils et les autres commandes : voir [LANCEMENT.md](LANCEMENT.md).
