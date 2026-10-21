# Lancement de la station météo

Fiche de commandes pour le groupe, à utiliser dans le terminal Linux de la VM
VirtualBox. Les commandes d'installation ci-dessous concernent Ubuntu/Debian.
Pour Arch Linux et le dépannage USB, consulter le
[guide VirtualBox complet](docs/INSTALLATION_VIRTUALBOX.md).

Exécuter les étapes dont on a besoin, sans copier tous les blocs d'un coup :
la démonstration, le tableau de bord et la console sont des usages distincts.

## 1. Installer les outils — une seule fois

```bash
sudo apt update
sudo apt install git curl usbutils build-essential python3 python3-venv python3-tk

python3 -m venv "$HOME/.venvs/station-meteo"
source "$HOME/.venvs/station-meteo/bin/activate"
python -m pip install platformio pyserial
```

## 2. Ouvrir le projet

Ouvrir un terminal dans le dossier `station-meteo-groupe1`, celui qui contient
`platformio.ini`, `src/` et `tools/`.

Si le projet n'a pas encore été récupéré, exécuter depuis le dossier où l'on
souhaite le placer, uniquement si `station-meteo-groupe1` n'y existe pas :

```bash
git clone https://github.com/bfrancizod/station-meteo-groupe1.git
cd station-meteo-groupe1
```

À chaque nouveau terminal, activer l'environnement Python :

```bash
source "$HOME/.venvs/station-meteo/bin/activate"
```

Pour mettre à jour une copie déjà clonée, depuis son dossier :

```bash
git pull --ff-only
```

## 3. Voir l'affichage graphique sans carte

```bash
python tools/station_dashboard.py --demo
```

La fenêtre présente des données fictives, identifiées par le bandeau
« DÉMONSTRATION ». Fermer cette fenêtre avant de lancer la connexion réelle.

## 4. Compiler le programme

```bash
pio run -e nucleo_l476rg
```

Attendre `[SUCCESS]`. Une connexion Internet est nécessaire pour télécharger
les outils lors de la première compilation.

## 5. Envoyer le programme sur la Nucleo — si nécessaire

Brancher la carte sur son connecteur ST-LINK avec un câble de données. Dans la
fenêtre VirtualBox, lui attribuer le périphérique via **Périphériques → USB →
ST-LINK / STMicroelectronics**. Fermer le tableau de bord et les moniteurs série.

```bash
pio run -e nucleo_l476rg -t upload
```

Cette commande écrit le programme sur la carte. Pour afficher les mesures d'une
carte déjà programmée, passer directement à l'étape suivante.

## 6. Afficher les vraies mesures

Vérifier que le ST-LINK est attribué à la VM, puis lancer :

```bash
python -m serial.tools.list_ports -v
python tools/station_dashboard.py
```

Dans la fenêtre du tableau de bord :

1. Choisir le port détecté, généralement `/dev/ttyACM0` ou `/dev/ttyACM1`.
   Ne pas conserver `/dev/ttyS31` s'il a été choisi automatiquement.
2. Cliquer sur **Connecter**.
3. Depuis le mode standard, maintenir le bouton **rouge environ 5 secondes**,
   puis le relâcher pour entrer en maintenance.
4. Attendre la réception des mesures ; un GPS sans position peut faire patienter
   jusqu'au délai `TIMEOUT`, de 30 secondes par défaut.

La maintenance fournit les blocs de mesures détaillés pour les courbes.
En standard/économie, la station écrit les mesures sur SD et affiche surtout les
bilans d'enregistrement. Depuis l'économie, un premier appui rouge de 5 secondes
revient au standard ; relâcher puis refaire un appui rouge pour la maintenance.

## 7. Utiliser la console série

Fermer le tableau de bord avant d'ouvrir la console. Adapter le nom du port
à celui détecté à l'étape 6. Utiliser en priorité le moniteur série PlatformIO.
Avec PlatformIO installé par l'extension IDE, la commande est :

```bash
~/.platformio/penv/bin/pio device monitor -p /dev/ttyACM0 -b 115200
```

Si ce chemin n'existe pas et que PlatformIO a été installé dans l'environnement
de cette fiche, utiliser le même moniteur depuis cet environnement :

```bash
source "$HOME/.venvs/station-meteo/bin/activate"
pio device monitor -p /dev/ttyACM0 -b 115200
```

Ces commandes permettent de lire les mesures et de saisir `HELP`, `STATUS`, etc.
Quitter avec le raccourci indiqué au démarrage du moniteur, généralement
**Ctrl+C**, pour libérer le port.

La commande Python avec miniterm reste une alternative ; la lancer après avoir
fermé le moniteur PlatformIO :

```bash
python -m serial.tools.miniterm /dev/ttyACM0 115200 --dtr 1 --rts 1 --exit-char 3
```

La commande également utilisée sur le poste initial est :

```bash
sudo /usr/bin/python -m serial.tools.miniterm /dev/ttyACM0 115200 --dtr 1 --rts 1 --exit-char 3
```

Elle lance le même miniterm, avec le Python système et les droits administrateur.
Le fichier `/usr/bin/python` doit exister et disposer de pyserial ; cette commande
n'utilise pas le Python de l'environnement activé. `sudo` est inutile lorsque
l'utilisateur a déjà les droits sur le port série.

Pour lire seulement les mesures avec les outils Linux, sans Python :

```bash
stty -F /dev/ttyACM0 115200 raw -echo && cat /dev/ttyACM0
```

| Méthode | Afficher les textes reçus | Envoyer `HELP` / `STATUS` | Python sur le PC |
|---|---|---|---|
| Moniteur PlatformIO, en priorité | Oui | Oui | Utilisé en interne par PlatformIO |
| Miniterm, avec ou sans `sudo` | Oui | Oui | Lancé explicitement |
| `stty` puis `cat` | Oui | Non, lecture seule | Non |

L'explication de chaque élément (`sudo`, `-m`, `--dtr`, `--rts`, `raw`, `&&`,
etc.) et des différences se trouve dans le
[comparatif détaillé des consoles](docs/INSTALLATION_VIRTUALBOX.md#comparaison-consoles).

Appuyer sur RESET de la Nucleo sans maintenir de bouton rouge/vert, puis saisir
`HELP` ou `STATUS` et valider avec Entrée si l'on utilise PlatformIO ou miniterm.
Pour afficher les mesures détaillées avec l'une des trois méthodes,
maintenir le bouton rouge 5 secondes depuis le mode standard afin d'entrer en
maintenance. Avec miniterm ou `cat`, **Ctrl+C** libère le port avant
de revenir au tableau de bord.

Un seul programme doit utiliser le port : console, moniteur PlatformIO ou
tableau de bord.

### Soutenance : continuer sans position GPS après 15 secondes

Compiler puis téléverser la version contenant cette option (étapes 4 et 5).
Dans la console PlatformIO ou miniterm, saisir les commandes suivantes, chacune
validée par Entrée ; ce sont des commandes de la station, pas du terminal Linux :

```text
GPS_DEMO=1
STATUS
```

`GPS_DEMO=1` active une option temporaire, disponible dans tous les modes.
Changer cette option annule l'acquisition en cours et en démarre une nouvelle,
sauf en configuration où les acquisitions restent suspendues.
Si aucune position satellite valide n'est obtenue pendant les **15 secondes de
l'acquisition**, la station utilise une latitude et une longitude fictives de
**0,000000°**, affichées avec la mention **(DEMO)**. Le délai général `TIMEOUT`
continue de s'appliquer aux autres capteurs.

Seule l'erreur GPS est effacée : la LED reprend la couleur fixe du mode si aucun
autre défaut n'est présent. Les défauts RTC, SD et autres capteurs restent signalés.
Une vraie position reçue avant le remplacement est toujours prioritaire ; le GPS
est réessayé à chaque nouvelle acquisition qui le demande. En économie, les
acquisitions qui ignorent volontairement le GPS conservent `NA`.

Pour présenter les courbes, passer en maintenance avec le bouton rouge, quitter
la console, puis ouvrir le tableau de bord sans appuyer sur RESET.
Les mesures des autres capteurs restent réelles. Le tableau de bord indique
**GPS : DÉMONSTRATION** pour les coordonnées fictives ; la SD et la console
conservent `(DEMO)`, et l'export CSV les identifie par la source `SERIE_GPS_DEMO`.
Cette option diffère de `--demo`, qui simule tout le tableau de bord sans carte.

Pour revenir au fonctionnement GPS normal :

```text
GPS_DEMO=0
```

L'option est **désactivée au redémarrage** et n'est pas enregistrée en Flash.
`STATUS` affiche `GPS_DEMO=1` lorsqu'elle est active et `GPS_DEMO=0` sinon.

## 8. Lancer les tests

Depuis la racine du projet :

```bash
python tools/station_dashboard.py --self-test
bash scripts/test_host.sh
```

Dans un environnement qui interdit LeakSanitizer, utiliser pour les tests C :

```bash
ASAN_OPTIONS=detect_leaks=0 bash scripts/test_host.sh
```

Ces tests vérifient le logiciel sur PC ; les essais sur la carte sont distincts.

## 9. En cas de problème

| Problème | Action |
|---|---|
| `pio: command not found` ou module Python absent | Réactiver l'environnement de l'étape 2. |
| Pas de fenêtre / erreur `DISPLAY` | Ouvrir le terminal depuis le bureau graphique de la VM. Tester `python -m tkinter`. |
| Aucun port de la Nucleo | Vérifier le câble de données et l'attribution du ST-LINK à la VM. |
| `Permission denied` | Suivre les étapes udev et groupe série du guide complet. |
| Port occupé | Fermer miniterm, le moniteur PlatformIO ou l'autre tableau de bord. |
| Connecté sans courbes | Passer en maintenance et attendre un bloc complet dans **Console technique**. |

Le [guide VirtualBox complet](docs/INSTALLATION_VIRTUALBOX.md) détaille les
permissions, la configuration USB et le fonctionnement de l'affichage.
