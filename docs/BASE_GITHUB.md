# Utilisation de la base officielle

## Origine vérifiée

- Dépôt : https://github.com/ValentinMoraine/Worldwide-Weather-Watcher.git
- Branche d'origine : main.
- Commit : `3c8fff4c410b22ee1b4ad279ee3a28e06ad8ae67`.
- Vérification : 18 octobre 2026, comparaison octet par octet avec l'archive
  Worldwide-Weather-Watcher-main fournie au début du projet : mêmes fichiers.

Cette version est construite à partir d'un clone de cette base. Tous les fichiers
suivis d'origine restent présents. Les ajouts applicatifs ne remplacent pas le
projet par un projet Arduino ni par une architecture indépendante.

## Fichiers inchangés et fichiers complétés

| Élément d'origine | Utilisation dans l'intégration |
|---|---|
| `src/main.c` | Même point d'entrée et même boucle ; ajout de l'initialisation et du traitement applicatif. |
| `src/setup.c` | Même rôle Global_Init et SysTick, UART/I2C/SPI ; initialisation restructurée, MSI 16 MHz, réception UART par interruptions et retarget printf tamponné. |
| `include/setup.h` | Conservé sans modification. |
| `include/cesi_types.h` | Conservé sans modification ; RTC_DateTime, GPS_Data et Env_Data utilisés par les pilotes. |
| `include/board_pins.h` | Mêmes broches ; ajout des options de polarité, couleur des boutons et SWAP GPS. |
| `grove_rgb.c` et son en-tête | Conservés sans modification ; LED réellement commandée par GroveRGB_SetColor. |
| `grove_light.c` et son en-tête | Interface conservée ; ajout de calibration ADC et délai de lecture raccourci. |
| `grove_button.c` et son en-tête | Interface conservée ; correction du pull incohérent avec une lecture active au niveau bas et polarité configurable. |
| `grove_rtc_ds1307.c` et son en-tête | Interface conservée ; validation du calendrier, du BCD, du bit CH et des formats 12/24 h. |
| `grove_gps_air530z.c` et son en-tête | Nom et fonctions historiques conservés ; parseur NMEA renforcé et extensions non bloquantes pour le GPS SIM28. |
| `grove_bme680.c` et son en-tête | Conservés pour la base ; non utilisés pour le BME280. Les valeurs constantes fictives de ReadEnvironment sont remplacées par un refus explicite. |
| `sd_logger.c` et son en-tête | Initialisation SPI et diagnostic bit-bang conservés. Attentes enrichies pour servir les boutons/GPS. WriteLine est implémentée dans le fichier compagnon sd_logger_file.c. |
| `platformio.ini` | Même carte nucleo_l476rg et framework stm32cube ; compléments pour bibliothèques, script de liaison et options de compilation. |
| `lib/library.json` | Fichier d'origine conservé ; copie du manifeste dans lib/cesi_grove pour le placer au niveau de la bibliothèque. |

Les autres fichiers de configuration suivis sont conservés. Le `.gitignore` est
complété pour les sorties de compilation et les tests.

## Comment les appels passent par la base

- `App_Init` et `App_Process` sont appelés par le `main.c` fourni.
- `Global_Init` appelle les initialisations Grove des boutons, du GPS et de la lumière.
- `Board_Red` et `Board_Green` appellent `GroveButton1_IsPressed` et `GroveButton2_IsPressed`.
- `Board_Light` appelle `GroveLight_ReadRaw`.
- `Time_Read` et `Time_Write` adaptent les types applicatifs vers `GroveRTC_GetDateTime`
  et `GroveRTC_SetDateTime`, qui utilisent RTC_DateTime.
- Le service GPS appelle `GroveGPS_Process` et `GroveGPS_Take`, extensions du pilote
  historique, qui produisent GPS_Data. Les anciennes ReadLine/ReadGGA restent disponibles.
- Le service d'environnement appelle `GroveBME280_*`, ajouté selon le modèle Grove,
  qui produit Env_Data avec le pilote de compensation Bosch.
- Le pilote de secteurs FatFs appelle **SDLogger_Init** avant d'ajouter la lecture
  de capacité et les commandes de transfert de secteurs.
- Après la gestion des révisions, le stockage appelle **SDLogger_WriteLine** pour
  l'ajout réel d'une ligne et la fermeture du fichier.
- Les couleurs et erreurs passent par le pilote RGB inchangé.

Les fonctions GPS bloquantes et le diagnostic SD bit-bang sont conservés pour
compatibilité, mais ne sont pas appelés dans la boucle périodique normale.
Ils restent disponibles dans les sources, comme les outils de diagnostic de la base.

## Pourquoi des ajouts sont nécessaires

Le dépôt fournit une boucle vide, un pilote d'environnement avec des valeurs
d'exemple et une fonction WriteLine qui renvoie toujours false. L'utiliser comme
base ne signifie donc pas le conserver inchangé pour annoncer un projet terminé.
Les modules applicatifs, la persistance, les fichiers FAT et les lectures réelles
sont des compléments nécessaires.

Le matériel photographié est différent de deux noms de pilotes du dépôt :
BME280 VMA335 et GPS SIM28. La partie NMEA du GPS peut être commune ; les
compensations BME280 et BME680, elles, ne sont pas interchangeables. C'est pourquoi
le BME280 a un fichier séparé à côté du pilote hérité.

## Ajouts principaux

Les fichiers d'application, de configuration, de formatage, de persistance et de
stockage se trouvent dans `src`. Les nouveaux types et interfaces publiques sont
dans `include`. Le pilote BME280 et la partie fichiers SD sont dans
`lib/cesi_grove`. Bosch et FatFs sont fournis comme bibliothèques complémentaires.
Le guide de mise en service, les tests PC et les scripts accompagnent le projet.

Les responsabilités du livrable 2 restent les mêmes. Pour cette implémentation,
les fonctions privées de modes, boutons, console et ordonnancement sont regroupées
dans app.c. Cette organisation est expliquée dans le guide ; elle ne change pas
les interfaces matérielles du dépôt pédagogique.
