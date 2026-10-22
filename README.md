# Worldwide Weather Watcher Groupe 1

Pour comprendre les composants, les fichiers et les choix du programme, lire le
[guide d'explication de la station](docs/COMPRENDRE_LA_STATION.md).

Les commandes pour compiler, programmer la carte et ouvrir l'affichage sont
regroupées dans la [fiche de lancement](LANCEMENT.md).

Pour installer le projet dans une VM et afficher les mesures, suivre le
[guide VirtualBox et tableau de bord graphique](docs/INSTALLATION_VIRTUALBOX.md).
Il couvre l'installation Linux, le passage USB du ST-LINK, les permissions,
la compilation et le lancement du tableau de bord avec ou sans carte.

Cette version complète **le dépôt pédagogique imposé** :
https://github.com/ValentinMoraine/Worldwide-Weather-Watcher

Base récupérée et vérifiée le 18 octobre 2026 :
`3c8fff4c410b22ee1b4ad279ee3a28e06ad8ae67`.
Les fichiers de cette base sont identiques à ceux de l'archive fournie initialement
par le groupe. Aucun fichier suivi de la base n'a été supprimé.

## Ce que ce paquet change

L'arborescence `src`, `include`, `lib/cesi_grove` et les interfaces publiques des
pilotes fournis sont conservées. Le programme passe par ces pilotes : ils ne sont
pas simplement rangés dans le dossier sans être utilisés.

- `main.c` garde `Global_Init` et sa boucle, complétés par `App_Init` et `App_Process`.
- `setup.c` reste le fichier d'initialisation et le point de réception des IRQ UART.
- `cesi_types.h` et le pilote RGB sont repris **sans modification**.
- Les pilotes boutons, lumière, RTC et GPS sont utilisés via leurs fonctions Grove.
- L'initialisation et le diagnostic SPI SD de `sd_logger.c` sont conservés ;
  les accès secteurs et les fichiers viennent les compléter.
- Le **BME280 VMA335** réellement présent est ajouté comme pilote voisin du BME680,
  avec le type `Env_Data` de la base. Il ne se fait pas passer pour un BME680.
- Le GPS **SIM28** utilise le pilote NMEA fourni sous le nom historique
  `grove_gps_air530z.c`, complété pour la réception continue et la validation GGA.

Le détail est dans `docs/BASE_GITHUB.md`. Le fichier
`docs/UPSTREAM_MANIFEST.json` contient le commit, les chemins et les empreintes
SHA256 d'origine, pour identifier précisément la base utilisée.

## Fonctions intégrées

- Quatre modes : standard, configuration, maintenance et économique.
- Appuis longs et filtrage des rebonds ; LED RGB et codes d'erreur.
- Température, humidité et pression réelles compensées par le pilote Bosch BME280.
- Luminosité analogique ramenée sur 10 bits.
- GPS NMEA GGA avec checksum, coordonnées et fraîcheur vérifiés.
- Lecture et réglage de la RTC, vérification du calendrier et de l'oscillateur.
- Paramètres conservés en Flash sur deux pages avec CRC.
- FAT16/FAT32 sur SD, journal _0 et copies de révision, avec vérification de copie.
- Console à 115200 bauds : HELP, STATUS, SHOW, VERSION, I2C, MEASURE et réglages.

L'arbitrage A1 du livrable est conservé : rouge 5 s en économie revient au standard ;
un nouvel appui après relâchement entre en maintenance. Le sujet attribuant deux
résultats au même événement, ce point reste à confirmer avec le tuteur.

## Ouvrir et compiler

1. Décompresser le paquet puis ouvrir dans VS Code le dossier
   **Worldwide-Weather-Watcher**, celui qui contient `platformio.ini`.
2. Utiliser l'extension PlatformIO pour compiler puis téléverser avec ST-LINK.
3. Ouvrir le moniteur à **115200 bauds, 8N1**, fin de ligne LF ou CRLF.
4. Suivre `docs/PRISE_EN_MAIN.md` pour contrôler les composants un à un.

Sous Linux, le script du projet permet :

```bash
chmod +x build.sh scripts/*.sh
./build.sh build
./build.sh upload
./build.sh monitor
```

Ne pas mélanger les fichiers de ce paquet avec ceux de la première version
réorganisée : le fichier `src/board.c` et les pilotes placés directement dans `src`
ont été réintégrés dans `setup.c` et `lib/cesi_grove`. Ajouter les deux variantes
produirait des définitions en double.

## Câblage utilisé par le programme

| Élément | Broches et prise |
|---|---|
| Luminosité | Grove A2, PA4 ; alimentation 3,3 V |
| Double bouton | Grove D2, PA10 et PB3 ; polarité configurable |
| RGB P9813 | Grove D6, entrée IN, PB10 horloge et PA8 données |
| GPS SIM28 | Grove A0, TX module vers PA0 et RX module vers PA1 ; SWAP UART4 |
| BME280 VMA335 | 3V3, GND, SCL PB8/D15, SDA PB9/D14, CSB 3V3, SDO GND |
| RTC | I2C PB8/PB9, adresse 0x68 ; alimentation à confirmer selon le module |
| SD | SPI1 PA5/D13, PA6/D12, PA7/D11 ; CS PB5/D4 |
| Console | USB ST-LINK, USART2 PA2/PA3 |

Les broches et les options sont dans `include/board_pins.h`. Une véritable puce
DS1307 demande normalement VCC=5 V : consulter le guide avant d'alimenter le module.
Ne pas passer tout le shield Grove en 5 V, en particulier à cause du BME280 et de
l'entrée analogique. Le connecteur ICSP du **shield SD** ne doit pas être confondu
avec le connecteur ST-LINK de la Nucleo.

## Tests et binaire livré

La compilation ARM de cette intégration et les trois suites de tests PC sont
exécutées. Le rapport exact est dans `docs/VALIDATION.md`. La maquette réelle
n'a pas été testée à distance et le chemin PlatformIO complet n'a pas été exécuté.
Les polarités, les tensions, la qualité des mesures et les fichiers SD doivent être
validés sur votre carte.

Le binaire correspondant aux sources se trouve dans `dist/firmware.bin`, avec
l'ELF et la carte mémoire. Téléverser seulement après vérification du câblage.

Tests sur PC avec un compilateur C :

```bash
./build.sh test
```

Dans un conteneur qui interdit LeakSanitizer :

```bash
ASAN_OPTIONS=detect_leaks=0 ./build.sh test
```

Compilation directe, sans lancer le client PlatformIO :

```bash
export SDK_STM32L4=/chemin/vers/STM32CubeL4
export ARM_GCC=/chemin/vers/arm-none-eabi-gcc
python3 scripts/build_direct.py
```

Le SDK doit fournir les dossiers `Drivers/STM32L4xx_HAL_Driver`,
`Drivers/CMSIS/Device/ST/STM32L4xx` et `Drivers/CMSIS/Include`. Si ces dépendances
existent dans `~/.platformio/packages`, le script les trouve automatiquement.
Il ne télécharge rien et n'appelle aucun service distant.

## Travail dans le dépôt du groupe

Le dépôt de Valentin Moraine est la base pédagogique ; les adaptations sont à
mettre dans **votre copie ou fork de groupe**, comme le demande le sujet.
Ce paquet ne modifie pas le dépôt distant de l'enseignant et ne publie rien.

Si votre dépôt de groupe contient déjà la base et aucun travail supplémentaire,
copier les fichiers de ce paquet à sa racine, contrôler le diff puis committer.
S'il contient déjà des modifications de collègues, comparer et intégrer les fichiers
concernés au lieu de les écraser. Ne pas copier un dossier `.git` venant d'ailleurs.

## Licences et origines

- Dépôt pédagogique et provenance exacte : `docs/BASE_GITHUB.md`.
- BME280 Bosch : commit dans `lib/bosch_bme280/ORIGIN.txt`, licence fournie.
- FatFs Elm-Chan : commit du miroir dans `lib/fatfs/ORIGIN.txt`, licence conservée
  dans les sources.
- CMSIS et HAL ST sont des dépendances de compilation non redistribuées dans ce
  paquet ; l'en-tête de configuration conserve ses mentions d'origine.
