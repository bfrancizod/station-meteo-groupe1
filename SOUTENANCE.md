# Soutenance : commandes essentielles

Fiche pour ton PC actuel. Brancher la Nucleo sur le connecteur ST-LINK.

**Commande pour ouvrir la console, taper `STATUS` et voir ce que tu tapes :**

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200 --echo
```

Fermer `cat`, les autres moniteurs et le tableau de bord avant de la lancer.

## 1. Ouvrir le projet dans un terminal Linux

```bash
cd ~/projets/station-meteo-groupe1
```

## 2. Afficher les textes de la carte dans la console

Lancer la commande PlatformIO indiquée tout en haut de cette fiche.

Cette console permet de lire les mesures **et de saisir des commandes**.
L'option `--echo` affiche les caractères que tu tapes : c'est l'**écho local**.
Par exemple, taper `STATUS` puis **Entrée** affiche l'état de la carte.
`--echo` affiche ta saisie ; la réponse de la carte arrive ensuite.

Autre méthode pour lire les textes : utiliser cette commande **à la place** du moniteur :

```bash
stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
```

Avec `cat`, la lecture seule reste active : pour envoyer `STATUS`, utiliser un
deuxième terminal avec `printf 'STATUS\r\n' > /dev/ttyACM0`.
Le `-echo` de `stty` concerne le port série ; il est différent de l'écho local du moniteur.

## 3. Passer le GPS en démonstration

**Avec le moniteur PlatformIO** : saisir directement ces lignes dans le moniteur,
chacune suivie d'Entrée :

```text
GPS_DEMO=1
STATUS
```

**Avec `cat`** : laisser l'affichage ouvert et envoyer l'activation depuis
**un deuxième terminal Linux** :

```bash
printf 'GPS_DEMO=1\r\nSTATUS\r\n' > /dev/ttyACM0
```

- `GPS_DEMO=1` : après 15 s sans position, utiliser `0,0 (DEMO)` et effacer le défaut GPS.
- `STATUS` : vérifier `GPS_DEMO=1` ; `ERR=0x00` signifie qu'aucun défaut n'est signalé.
- Les autres capteurs restent réels et leurs défauts restent signalés.

**Après RESET ou débranchement, refaire cette activation.**

## 4. Voir d'abord les mesures dans la console

Depuis le mode standard, maintenir le **bouton rouge 5 secondes**, puis le relâcher :
la station passe en maintenance. Attendre environ **15 secondes** pour les mesures.
La LED est orange fixe si aucun défaut n'est présent.

Le terminal affiche la date, la température, l'humidité, la pression, la luminosité,
puis les coordonnées GPS. Sans position satellite, elles deviennent :

```text
Latitude     : 0.000000 (DEMO)
Longitude    : 0.000000 (DEMO)
```

## 5. Passer ensuite à l'affichage graphique

Quitter le moniteur ou `cat` avec **Ctrl+C** pour libérer le port,
puis lancer dans le terminal Linux ouvert dans le dossier du projet :

```bash
/usr/bin/python tools/station_dashboard.py
```

Choisir `/dev/ttyACM0`, cliquer sur **Connecter** et attendre les mesures.
**Ne pas appuyer sur RESET entre l'activation du GPS de soutenance et les courbes.**

## Si le programme doit être mis à jour avant la présentation

Fermer la console et le tableau de bord. Compiler et envoyer le code sur la carte :

```bash
~/.platformio/penv/bin/pio run -e nucleo_l476rg -t upload
```

Puis reprendre à l'étape 2. Pour les autres commandes : voir [LANCEMENT.md](LANCEMENT.md).
