# Utiliser la station météo dans VirtualBox et afficher les mesures

Ce guide permet au groupe de préparer une VM Linux, compiler le programme de la
NUCLEO-L476RG et lancer le tableau de bord `tools/station_dashboard.py`.
La procédure principale vise une VM Ubuntu/Debian avec un bureau graphique ;
les commandes d'installation pour Arch Linux sont également indiquées.

Le programme C s'exécute sur la Nucleo. Le tableau de bord Python s'exécute dans
la VM et reçoit les mesures par le câble USB du ST-LINK.

```text
Capteurs → Nucleo → USB ST-LINK → VirtualBox → port série Linux → tableau de bord
```

**Repère :** « hôte » désigne le PC qui lance VirtualBox ; « VM » désigne le
Linux qui tourne dans sa fenêtre. Sauf mention « sur l'hôte », saisir les
commandes de ce guide dans un terminal du bureau de la VM.

## 1. Récupérer le projet dans la VM

Copier le dossier reçu du groupe dans le dossier personnel de la VM, par exemple
`~/projets/station-meteo-groupe1`. Si le projet est déjà présent, utiliser sa copie
existante sans l'écraser. Le dossier à ouvrir contient `platformio.ini`, `src/`,
`include/`, `lib/` et `tools/`.

Si l'on préfère récupérer le projet avec Git, la commande de clonage est donnée
à l'étape 2, après l'installation de Git.

Un clone récupère uniquement les commits déjà publiés sur GitHub. Pour partager
une version qui n'a pas encore été poussée, transmettre le dossier ou une archive
de cette version au groupe. Vérifier que `tools/station_dashboard.py` y figure.
Il n'est pas nécessaire de copier `.pio/`, les environnements Python ou les
sauvegardes `.bak` : les outils locaux se réinstallent dans chaque VM.

Travailler dans le disque de la VM simplifie les permissions. Un dossier partagé
VirtualBox peut servir au transfert ; copier ensuite le projet dans le dossier
personnel de la VM.

## 2. Installer les outils dans la VM

La VM doit avoir accès à Internet pour installer les paquets et télécharger les
outils STM32 lors de la première compilation. Le mode réseau NAT de VirtualBox
convient pour ces téléchargements.

### Ubuntu / Debian

```bash
sudo apt update
sudo apt install git curl usbutils build-essential python3 python3-venv python3-tk
```

### Arch Linux

```bash
sudo pacman -Syu --needed git curl usbutils base-devel python python-pip tk
```

### Clonage si le projet n'a pas déjà été copié

Après l'installation de Git, on peut cloner le dépôt **si le dossier de
destination n'existe pas** :

```bash
mkdir -p "$HOME/projets"
git clone https://github.com/bfrancizod/station-meteo-groupe1.git "$HOME/projets/station-meteo-groupe1"
```

### Environnement Python commun aux commandes suivantes

Créer un environnement dans le dossier personnel, puis y installer PlatformIO et
pyserial. Tkinter vient du paquet système `python3-tk` ou `tk` installé ci-dessus.

```bash
python3 -m venv "$HOME/.venvs/station-meteo"
source "$HOME/.venvs/station-meteo/bin/activate"
python -m pip install --upgrade pip
python -m pip install platformio pyserial
python -c "import tkinter, serial; print('Tkinter et pyserial : OK')"
pio --version
```

**À chaque nouveau terminal**, réactiver cet environnement avant d'utiliser
`python` ou `pio` :

```bash
source "$HOME/.venvs/station-meteo/bin/activate"
cd "$HOME/projets/station-meteo-groupe1"
```

Adapter le `cd` si le groupe a choisi un autre dossier. Utiliser `python` après
activation de l'environnement ; `/usr/bin/python` contournerait cet environnement.
Lancer les outils avec le compte habituel. `sudo` sert ici à installer les paquets
et les règles système, pas à démarrer le tableau de bord ou PlatformIO.

Le guide utilise une installation Python isolée. La documentation officielle
décrit aussi l'[installation de PlatformIO](https://docs.platformio.org/en/stable/core/installation/index.html)
et son [installation avec pip](https://docs.platformio.org/en/stable/core/installation/methods/pypi.html).

## 3. Vérifier l'affichage graphique sans carte

Dans le bureau graphique de la VM, lancer :

```bash
python -m tkinter
```

Une petite fenêtre de test doit apparaître ; la fermer avant de continuer.
C'est le [test de Tkinter prévu par Python](https://docs.python.org/3/library/tkinter.html).

Depuis la racine du projet :

```bash
python tools/station_dashboard.py --self-test
python tools/station_dashboard.py --demo
```

La première commande doit annoncer que les tests des formats et du parseur sont
corrects. La seconde ouvre le tableau de bord avec des **données fictives** et un
bandeau « DÉMONSTRATION ». La carte et le port USB ne sont pas nécessaires.
Fermer cette fenêtre avant de passer à la vraie station.

La fenêtre utilise Tkinter et ses courbes sont dessinées avec Canvas. Aucun
serveur web, navigateur, Matplotlib ou Node.js n'est requis. Prévoir une résolution
confortable, par exemple 1280 × 900, pour la fenêtre de 1100 × 850 du programme.
Les additions invité VirtualBox peuvent faciliter le redimensionnement du bureau.

Si `no display name and no $DISPLAY environment variable` apparaît, le terminal
n'a pas accès à une session graphique. Ouvrir un terminal depuis le bureau Linux
visible dans VirtualBox ; une VM uniquement en console ne peut pas afficher cette
fenêtre telle quelle. Ne pas inventer une valeur de `DISPLAY` pour masquer le
problème.

## 4. Donner le périphérique USB à la VM

### Réglages VirtualBox sur l'hôte

1. Éteindre complètement la VM pour modifier son contrôleur USB.
2. Dans les paramètres de la VM, ouvrir **USB**, activer le contrôleur et choisir
   **USB 3.0 / xHCI** pour une VM Linux récente.
3. Brancher la Nucleo avec un câble de données sur son connecteur **ST-LINK**.
4. Ajouter un filtre USB depuis le périphérique ST-LINK / STMicroelectronics
   détecté. Le filtre permet sa capture automatique par la VM.
5. Démarrer la VM. Au besoin, sélectionner le ST-LINK dans le menu de sa fenêtre
   **Périphériques → USB**. Après un débranchement, vérifier cette sélection.

Cette capture donne le périphérique à la VM et le retire à l'hôte. Fermer les
moniteurs série et logiciels de programmation qui l'utilisent sur l'hôte.
Avec un hôte Windows, rebrancher la carte après la création du filtre si nécessaire.
Voir la [procédure USB officielle de VirtualBox](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/working-with-vms.html).

Sur VirtualBox 7 et versions suivantes, les contrôleurs USB EHCI/xHCI sont inclus
dans le paquet de base : l'Extension Pack n'est pas un prérequis pour ce passage
USB local. Ce changement est décrit dans les
[notes de version 7.0 d'Oracle](https://docs.oracle.com/en/virtualization/virtualbox/7.0/relnotes/EN-VBOX-7-0-RELNOTES.pdf).
Les additions invité et l'Extension Pack sont deux composants différents.

**Si l'hôte est Linux**, vérifier sur l'hôte que son utilisateur appartient au
groupe `vboxusers`. Si nécessaire, exécuter **sur l'hôte** :

```bash
sudo usermod -aG vboxusers "$USER"
```

Fermer puis rouvrir la session de l'hôte pour appliquer le changement. Ce groupe
est distinct du groupe d'accès au port série à régler dans la VM à l'étape 5.
Source : [installation Linux de VirtualBox](https://docs.oracle.com/en/virtualization/virtualbox/7.2/user/installation.html).

### Détection dans la VM

```bash
lsusb
python -m serial.tools.list_ports -v
```

Le ST-LINK doit apparaître dans `lsusb`, et son port série est généralement
`/dev/ttyACM0`, parfois `/dev/ttyACM1`. Utiliser le nom réellement affiché. Un nom
`COM...` sur l'hôte Windows n'est pas le nom du port dans la VM Linux.

La rubrique « Ports série » des paramètres VirtualBox n'est pas nécessaire pour
cette méthode : on transmet le périphérique USB ST-LINK complet.

## 5. Autoriser l'accès USB et série dans la VM

Installer les règles udev pour l'accès au ST-LINK, notamment pour le téléversement :

```bash
curl -fsSL https://raw.githubusercontent.com/platformio/platformio-core/develop/platformio/assets/system/99-platformio-udev.rules -o "$HOME/99-platformio-udev.rules"
sudo install -m 644 "$HOME/99-platformio-udev.rules" /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo udevadm trigger
```

Débrancher/rebrancher la Nucleo puis vérifier qu'elle est de nouveau attachée à
la VM. Procédure de référence : [règles udev PlatformIO](https://docs.platformio.org/en/stable/core/installation/udev-rules.html).

Si le port série indique `Permission denied`, examiner ses droits dans la VM :

```bash
ls -l /dev/ttyACM0
id
```

Remplacer `ttyACM0` si nécessaire. Selon le groupe indiqué par `ls -l`, utiliser
**l'une** de ces commandes dans la VM :

```bash
# Ubuntu/Debian : si le groupe du port est dialout
sudo usermod -aG dialout "$USER"
```

```bash
# Arch Linux : si le groupe du port est uucp
sudo usermod -aG uucp "$USER"
```

Fermer puis rouvrir la session de la VM, ou la redémarrer ; vérifier les groupes
avec `id`, puis réactiver l'environnement Python. Changer les permissions avec
`chmod 777` n'est pas nécessaire et ne constitue pas un réglage durable.

## 6. Compiler et, si nécessaire, programmer la Nucleo

La compilation n'est pas nécessaire pour afficher les données d'une carte déjà
programmée avec cette version du projet. Pour préparer le programme depuis les
sources, exécuter à la racine du dépôt :

```bash
pio run -e nucleo_l476rg
```

Attendre `[SUCCESS]`. La première compilation télécharge les outils et le SDK ST.
`platformio.ini` sélectionne déjà la NUCLEO-L476RG, STM32Cube et ST-LINK ; les
bibliothèques Bosch/Grove/FatFs sont fournies dans `lib/`.

Si la carte doit être programmée, fermer le tableau de bord et tous les moniteurs
série, puis exécuter :

```bash
pio run -e nucleo_l476rg -t upload
```

Cette dernière commande écrit le programme sur la carte. Utiliser le résultat de
la compilation actuelle plutôt que supposer que `dist/firmware.bin` a été mis à
jour : ce fichier livré n'est pas automatiquement remplacé par `pio run`.
Si le périphérique USB se reconnecte pendant l'opération, le rattacher à la VM.

Pour travailler dans VS Code, ouvrir le dossier contenant `platformio.ini` et
installer l'extension PlatformIO IDE. Son terminal PlatformIO dispose de ses
propres outils ; le terminal préparé avec l'environnement ci-dessus fonctionne
aussi sans VS Code. Pour lancer le tableau de bord, conserver le Python qui
dispose de Tkinter et pyserial.

## 7. Vérifier la console avant le tableau de bord

Le tableau de bord doit être fermé pour cette vérification. Remplacer le port si
la détection a donné un autre nom :

```bash
python -m serial.tools.miniterm /dev/ttyACM0 115200 --dtr 1 --rts 1 --exit-char 3
```

Appuyer brièvement sur le bouton RESET de la Nucleo, sans maintenir de bouton
rouge/vert : le démarrage normal sélectionne le mode standard. Saisir `HELP`,
puis `STATUS`, en validant chaque commande avec Entrée. Les réponses confirment
que la console fonctionne. `--exit-char 3` permet de quitter avec **Ctrl+C** et
de libérer le port.

Référence des commandes PC : [outils pyserial](https://pyserial.readthedocs.io/en/latest/tools.html).
Ne pas lancer miniterm, le moniteur PlatformIO et le tableau de bord simultanément
sur le même port.

## 8. Afficher les mesures réelles

Après avoir quitté le moniteur série :

```bash
python tools/station_dashboard.py
```

1. Cliquer sur **Actualiser** si nécessaire.
2. Choisir le vrai port de la Nucleo, par exemple `/dev/ttyACM0`. Le programme
   peut sélectionner automatiquement un autre port, comme `/dev/ttyS31` :
   corriger manuellement ce choix.
3. Cliquer sur **Connecter**. La fenêtre doit indiquer la connexion au port choisi.
4. Depuis le mode standard, maintenir le bouton **rouge environ 5 secondes**,
   puis le relâcher : la station entre en maintenance.
5. Ouvrir l'onglet **Console technique** : attendre le bloc de mesures contenant
   notamment `Date / heure`, `Mode`, `Temperature`, `Humidite` et `Longitude`.
   Les valeurs et les courbes se remplissent à réception d'un bloc complet.

Le délai dépend de l'acquisition : un GPS sans position peut faire attendre
jusqu'au paramètre `TIMEOUT`, qui vaut 30 secondes par défaut mais peut avoir été
modifié et sauvegardé. La maintenance programme ensuite le cycle suivant une
seconde après la fin du précédent. Elle n'écrit aucune mesure sur SD.

En **standard/économie**, le programme affiche surtout le bilan `CYCLE ... SD=...`
et écrit les mesures sur SD. Une connexion série réussie ne suffit donc pas à
faire apparaître les cartes de mesures : passer en maintenance pour les blocs
détaillés. Depuis l'économie, un premier appui rouge de 5 s revient au standard ;
relâcher puis refaire un appui rouge de 5 s pour entrer en maintenance.

Le tableau de bord accepte le format compact et le format lisible. Une donnée
`NA` devient une valeur indisponible, pas zéro. Une position GPS peut rester
indisponible en intérieur ; le pilote applicatif attend une phrase **GGA** valide.
`SD : inconnu` signifie qu'aucun message exploitable sur la SD n'a encore été reçu.
Les dernières valeurs restent visibles après déconnexion : consulter leur âge.

Le bouton **Exporter CSV** sauvegarde les acquisitions reçues dans la VM. Pour
les rendre accessibles sur l'hôte, choisir un dossier partagé ou copier le CSV
après l'export. Le tableau de bord ne lit pas directement les fichiers `.LOG`
présents sur la carte SD.

En configuration, le tableau de bord envoie `STATUS` toutes les 5 secondes tant
qu'il est connecté. Cette activité série entretient le délai d'inactivité : le
retour automatique après 30 minutes ne se produira pas pendant ces échanges.
Pour les réglages, utiliser la console et le [guide de prise en main](PRISE_EN_MAIN.md).
Conserver les branchements et l'association actuelle des boutons : les sources
utilisent `RED_IS_BUTTON1=0` ; la préparation de la VM ne demande pas de modifier
`board_pins.h`.

## 9. Dépannage rapide

| Symptôme | Vérification / action |
|---|---|
| `No module named serial` | Réactiver l'environnement puis exécuter `python -m pip install pyserial` avec ce même Python. |
| `No module named tkinter` ou `_tkinter` | Installer `python3-tk` dans Ubuntu/Debian, ou `tk` dans Arch, puis vérifier `python -m tkinter`. Tkinter ne s'installe pas avec `pip install tkinter`. |
| Aucune fenêtre / erreur `DISPLAY` | Ouvrir le terminal depuis la session graphique de la VM ; tester d'abord Tkinter. |
| Le ST-LINK n'apparaît pas dans `lsusb` de la VM | Vérifier le câble de données, le connecteur ST-LINK et la capture dans **Périphériques → USB**. Sur un hôte Linux, vérifier aussi `vboxusers` sur l'hôte. |
| ST-LINK visible, mais aucun `ttyACM` | Examiner `sudo dmesg` après reconnexion pour vérifier la détection du port série USB ; refaire la capture si nécessaire. |
| `Permission denied` | Vérifier les règles udev et le groupe du port dans la VM, puis rouvrir la session. |
| Port occupé / `Device or resource busy` | Fermer le moniteur VS Code/PlatformIO, miniterm et toute autre instance du tableau de bord. |
| Connexion impossible sur `ttyS...` | Refaire `python -m serial.tools.list_ports -v` et sélectionner manuellement le ST-LINK. |
| Port disparu après débranchement ou programmation | Rattacher le ST-LINK à la VM, relister les ports et reconnecter le tableau de bord. |
| Connecté, mais aucune courbe | Vérifier le mode maintenance et la réception d'un bloc complet dans **Console technique** ; attendre le délai d'acquisition. |
| `pio: command not found` | Réactiver l'environnement Python, ou utiliser le terminal de l'extension PlatformIO. |
| `[SUCCESS]` à la compilation mais aucune connexion carte | La compilation ne vérifie ni le passage USB, ni le téléversement, ni les droits série. Contrôler les étapes 4 à 8. |
| `SD=ERREUR` ou GPS `NA` | Examiner `STATUS` et le guide matériel : une RTC invalide peut empêcher l'enregistrement, et le GPS peut manquer de réception. Ce n'est pas automatiquement un problème de VM. |

## 10. Routine pour les séances suivantes

Après l'installation initiale, démarrer la VM, lui attribuer le ST-LINK et ouvrir
un terminal du bureau :

```bash
source "$HOME/.venvs/station-meteo/bin/activate"
cd "$HOME/projets/station-meteo-groupe1"
python -m serial.tools.list_ports -v
python tools/station_dashboard.py
```

Sélectionner le port, connecter le tableau de bord et passer la station en
maintenance. Pour finir, cliquer sur **Déconnecter**, fermer la fenêtre et
attendre la fin de toute programmation avant de relâcher l'USB à l'hôte.

Pour vérifier le logiciel sans utiliser le matériel :

```bash
python tools/station_dashboard.py --self-test
bash scripts/test_host.sh
```

Ces contrôles ne valident pas le passage USB VirtualBox ou les capteurs réels.
La réception des mesures et le téléversement doivent être essayés dans la VM
du groupe. Aucune modification du code C ou Python n'est nécessaire pour suivre
ce guide.
