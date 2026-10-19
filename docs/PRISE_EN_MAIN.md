# Mise en service et explication du programme

## 1 Vérifier la maquette avant de téléverser

La photographie finale doit montrer les étiquettes des prises Grove, le sélecteur
3V3, le branchement Dupont du BME280 et les trois liaisons ICSP du shield SD.
Le logiciel ne peut pas corriger un échange entre alimentation et signal.

**Point à corriger par rapport aux premiers conseils de branchement : une vraie
puce DS1307 demande VCC entre 4,5 et 5,5 V selon sa fiche constructeur.** La
compatibilité du module Grove complet dépend de sa révision. Un DS1307 alimenté
en 3,3 V, surtout avec une pile de 3 V, peut refuser les accès I2C. Il faut
identifier sa référence et son circuit d'alimentation avant de le considérer
validé. Ne pas passer tout le shield Grove sur 5 V : le BME280 et la sortie
analogique doivent rester compatibles avec les entrées 3,3 V. Si la RTC nécessite
5 V, utiliser une alimentation séparée avec masse commune et une adaptation I2C
appropriée, ou un module RTC compatible 3,3 V. Aucun pull-up 5 V ne doit se retrouver
sur le BME280. Confirmer aussi le type et la tension de la pile du module.

Source constructeur DS1307 :
https://www.analog.com/media/en/technical-documentation/data-sheets/ds1307.pdf

Le BME280 VMA335 ne tolère pas une alimentation de 5 V. Les broches sont nommées
sur son circuit : VCC, GND, SCL, SDA, CSB, SDO. Le brochage du README suppose
CSB relié à 3V3 et SDO à GND. Le pilote cherche les adresses 0x76 puis 0x77.
La LED doit recevoir son câble sur IN, pas sur OUT.

## 2 Ouvrir et compiler dans VS Code

Ouvrir le dossier `Worldwide-Weather-Watcher`, celui qui contient `platformio.ini`, et non
uniquement le dossier `src`. Installer l'extension PlatformIO IDE si nécessaire.
Les bibliothèques BME280 et FatFs sont déjà présentes ; aucune bibliothèque
Arduino n'est nécessaire. Les fichiers C du SDK ST sont apportés par PlatformIO.
Le script de liaison réserve 4 Ko de Flash pour les réglages, 8 Ko de pile et 4 Ko de tas pour les conversions numériques de newlib-nano.

Sous Arch Linux : utiliser les règles udev PlatformIO/ST-LINK pour l'accès USB.
En cas de refus du port série, vérifier son groupe avec `ls -l /dev/ttyACM*` et
les groupes avec `id` ; utiliser le groupe réellement affiché (souvent uucp sur
Arch, dialout sur Ubuntu). Reconnexion de session après changement de groupe.
Dans VirtualBox : connecter le périphérique ST-LINK à la VM depuis le menu USB ;
il ne peut pas être utilisé simultanément par l'hôte et la VM.

Le moniteur série est réglé à 115200 bauds. Ouvrir le moniteur puis appuyer sur
RESET pour voir les messages de démarrage. Un fichier de configuration absent
charge les défauts ; cela n'est pas une preuve de panne Flash.

## 3 Tester chaque composant

### Console et boutons

Saisir `HELP`, puis `STATUS` avec une fin de ligne. STATUS donne le mode, le masque
d'erreurs, les niveaux PA10/PB3 et les états logiques RED/GREEN.

Faire STATUS avec les deux boutons relâchés, puis avec chaque bouton maintenu
brièvement. RED doit être 1 seulement avec le rouge appuyé, GREEN seulement avec
le vert appuyé. Si les couleurs sont inversées, changer RED_IS_BUTTON1 dans
`include/board_pins.h`. Si la logique est inversée, changer BUTTON_ACTIVE_LOW.
Recompiler et téléverser après toute modification. Le binaire fourni utilise
RED_IS_BUTTON1=1, BUTTON_ACTIVE_LOW=1 et GPS_SWAP_PINS=1 ; ces valeurs ne sont pas
attestées par un test sur votre module.

### Bus I2C et BME280

La commande `I2C` doit trouver 68 pour la RTC et 76 ou 77 pour le BME280. Cette
commande constate une réponse à l'adresse, pas la validité d'une mesure.
`MEASURE` demande un cycle immédiat en standard, économique ou maintenance. Elle
est refusée en configuration et pendant un cycle déjà actif. C'est une commande
de diagnostic supplémentaire ; elle ne modifie pas LOG_INTERVAL.

Pour voir les mesures, entrer en maintenance par le bouton rouge de 5 s depuis
standard. La ligne doit donner des valeurs plausibles ; chauffer légèrement le
capteur avec la main et masquer le capteur de lumière permettent de voir varier
les mesures. Le BME280 retourne trois mesures réelles compensées par son pilote
Bosch, aucune constante d'exemple.

### GPS

Placer l'antenne près d'une fenêtre ou à l'extérieur avec une vue dégagée. Le
premier fix peut dépasser TIMEOUT : NA est alors normal tant qu'aucune position
valide n'arrive. Le GPS doit transmettre du NMEA GGA à 9600 bauds.
PA0 reçoit le TX du GPS grâce au SWAP UART4. Si votre câblage est croisé autrement,
ajuster GPS_SWAP_PINS ou corriger le câblage. Ne pas brancher le GPS au port Grove
UART qui utilise ici la console USART2.

Le parseur vérifie le checksum, conserve les champs vides et refuse les coordonnées
incohérentes. Une position zéro est valide. Un cycle ne reprend pas la position
ancienne d'un cycle précédent. En économie, les cycles 1,3,5... demandent le GPS ;
les autres inscrivent NA. Le module reste alimenté et les octets sont drainés pour
éviter une saturation ; aucune commande spécifique de mise en veille SIM28 n'est
envoyée.

### RTC et configuration

Démarrer en maintenant le rouge pour entrer en configuration. Si l'horloge est
invalide, régler d'abord une date réelle, puis l'heure réelle, par exemple :

```text
DATE=10,16,2026
CLOCK=16:30:00
DAY=FRI
STATUS
```

L'exemple ne doit pas être réutilisé comme heure actuelle. La station ne règle
jamais la RTC automatiquement à la date de compilation. Si une RTC invalide
reçoit CLOCK seul, l'heure est seulement conservée en attente d'une DATE explicite,
et la console indique que l'horloge n'est pas encore réglée.

Les commandes suivantes servent à un essai accéléré, et sont persistantes :

```text
LOG_INTERVAL=1
TIMEOUT=10
FILE_MAX_SIZE=512
SHOW
```

Les réglages hors limites sont refusés. Une valeur n'est annoncée sauvegardée que
si la Flash a été écrite et vérifiée. Redémarrer sans maintenir de bouton pour
retrouver le mode standard. Rétablir ensuite les défauts en configuration avec
RESET, puis redémarrer normalement. Le mode configuration se ferme aussi après
30 minutes sans activité série. RESET ne supprime pas les fichiers SD et ne change
pas la RTC. VERSION affiche version et numéro de lot.

### Carte SD et fichiers

Préparer une carte connue fonctionnelle en FAT16 ou FAT32 sur un ordinateur.
Le firmware ne formate jamais la carte. Le support testé logiquement est FAT16/FAT32,
512 octets par secteur, carte SD ou SDHC. exFAT n'est pas activé.
Insérer une carte avant le démarrage en mode standard. Après un cycle complet,
la console indique CYCLE ... SD=OK ou ERREUR. Une RTC invalide empêche l'écriture
sans date et produit ERR_RTC, même si la carte elle-même fonctionne.

Passer en maintenance et **attendre le message SD liberee** avant tout retrait.
Une demande de mode ne coupe pas brutalement une transaction SD déjà en cours.
Après lecture sur ordinateur, replacer la carte puis maintenir rouge 5 s pour
revenir au mode de mesure. Le logiciel tente de remonter le volume.

Le fichier actif ressemble à `261016_0.LOG`. Une ligne contient :

```text
date_heure;mode;temperature_C;humidite_pct;pression_hPa;luminosite_0_1023;latitude;longitude
```

Cette ligne descriptive est la légende du format, pas un en-tête écrit dans chaque
fichier. Le point est le séparateur décimal. Toute mesure non valide devient NA.
Les révisions positives sont des copies successives ; _0 reste le fichier actif.
Leur copie est relue avant de tronquer _0. Un temporaire `.TMP` restant après une
coupure bloque volontairement la rotation correspondante : sauvegarder les fichiers
sur PC, comparer TMP et LOG puis traiter manuellement le temporaire. Rien n'est
supprimé automatiquement pour gagner de la place. La FAT ne garantit pas la
résistance à toutes les coupures pendant une écriture.

## 4 Modes et ambiguïté du sujet

- Standard : démarrage normal, vert, période par défaut 10 min, mesures sur SD.
- Configuration : rouge au démarrage, jaune, réglages série, capteurs non acquis.
- Économique : vert 5 s depuis standard, bleu, période doublée et GPS un cycle sur deux.
- Maintenance : rouge 5 s depuis standard, orange, mesures série et SD libérée.
- Sortie maintenance : rouge 5 s, retour au mode mémorisé après tentative de remontage.

L'hypothèse A1 du livrable est implémentée : rouge 5 s en économie revient au
standard. Relâcher puis refaire rouge 5 s pour accéder à maintenance. Le sujet
attribue deux résultats au même appui ; le tuteur doit confirmer cet arbitrage
avant validation finale. Un bouton maintenu produit un seul événement.

Le mode économique réduit la fréquence des mesures. Il ne met pas le STM32 en STOP
et ne coupe pas matériellement l'alimentation des modules.

## 5 Comprendre les défauts

STATUS affiche ERR en hexadécimal ; les bits s'additionnent :

| Valeur | Défaut |
|---|---|
| 0x01 | RTC absente, arrêtée ou date invalide |
| 0x02 | GPS indisponible après deux acquisitions demandées en timeout |
| 0x04 | Capteur indisponible après deux acquisitions en timeout |
| 0x08 | Valeur de capteur hors seuil ou incohérente |
| 0x10 | SD pleine confirmée par FatFs |
| 0x20 | Accès SD, écriture ou archivage impossible |

La LED alterne rouge et la couleur du défaut sur un cycle complet de 1 s.
RTC : bleu ; GPS : jaune ; capteur : vert ; SD : blanc. Pour une donnée incohérente
ou une erreur d'écriture SD, le vert ou le blanc dure deux fois le rouge.
La priorité d'affichage est RTC, erreur SD, SD pleine, accès capteur, incohérence,
GPS. Les autres défauts restent présents dans STATUS.

Les lectures ratées sont réessayées jusqu'à TIMEOUT. Les canaux demandés en échec
sont notés NA. Les canaux désactivés et le GPS volontairement ignoré ne comptent
pas comme un nouveau timeout. Les données valides des autres canaux sont gardées.
Un capteur rétabli efface son défaut à la fin d'un cycle valide.

## 6 Comment le code fonctionne

`main.c` prépare la carte puis appelle continuellement App_Process. Le compteur
HAL_GetTick mesure les durées ; la RTC fournit exclusivement la date et l'heure.
L'horloge processeur est explicitement MSI 16 MHz dans cette version, pas 80 MHz.

`setup.c` reçoit les octets USART2/UART4 dans des anneaux de 1024 octets. Les
interruptions ne lisent aucun capteur et n'accèdent pas à FatFs. La console TX est
aussi tamponnée. RX_TX_LOST signale des octets perdus ou des erreurs UART ; éviter
les envois série massifs pendant les opérations Flash/SD.

`environment.c` appelle le nouveau pilote Grove BME280, qui lance le capteur en conversion forcée. Le superviseur attend au
moins 100 ms en continuant les autres tâches, puis lit les trois valeurs. Le pilote
Bosch lit les coefficients propres à la puce et applique les compensations.
La pression est convertie des pascals en hectopascals.

`app.c` fige les paramètres d'un cycle en pratique, car les réglages ne sont
acceptés qu'en configuration où aucun cycle n'est actif. Le cycle lit la lumière,
le BME280 et un nouveau fix GPS. Il vérifie ensuite les seuils et horodate la mesure.
Les transferts I2C/ADC courts ont des délais bornés. La SD peut prendre plus
longtemps ; pendant ses attentes, les boutons et le GPS sont traités. Les commandes
série sont exécutées seulement au retour dans la boucle, pour ne pas réentrer dans
FatFs.

`nv_store.c` conserve deux enregistrements avec version, compteur, CRC et marqueur
final. Une nouvelle configuration est écrite sur l'autre page, sans effacer
l'ancienne avant validation. Le format de Config est propre à la version 1 ; un
changement de disposition impose de changer la version persistante. Le code
refuse la sauvegarde si le contrôleur Flash n'est pas en mode double banque ou si le démarrage depuis la seconde banque est activé.
Les options de la carte ne sont pas modifiées par le firmware.

`storage.c` utilise FatFs, et `diskio_spi.c` convertit ses lectures/écritures de
secteurs en commandes SD SPI. Les transferts sont vérifiés ; le pilote SPI actuel
n'active pas la vérification CRC des données SD. Les durées de blocage maximales,
la tenue aux perturbations et la consommation électrique restent à mesurer sur
la maquette. Un pilote de production demanderait une qualification supplémentaire.

## 7 Adapter ensuite le programme

Le brochage et les options électriques sont dans board_pins.h. Les types partagés
sont dans app_types.h, les prototypes publics dans services.h. Les fonctions
privées d'app.c correspondent aux responsabilités modes, planificateur, boutons,
console et acquisition du livrable 2 ; elles peuvent être déplacées vers des fichiers
séparés sans changer les pilotes.

Pour un nouveau capteur : ajouter un pilote, son identifiant SensorId, sa valeur
dans Measurement et sa colonne de formatage ; prévoir validation et délai. Les
adresses I2C et broches libres doivent être contrôlées, notamment PA0/PA1 déjà
réservées au GPS. Le potentiomètre facultatif A3 n'est pas utilisé par le firmware.

`syscalls.c` borne le petit tas utilisé par la bibliothèque C pour convertir les nombres réels. Les données applicatives et les tampons restent statiques. Un débordement de tas est refusé au lieu de corrompre la pile.

## 8 Provenance du code

Cette édition réintègre les compléments dans la base GitHub officielle, commit
3c8fff4c410b22ee1b4ad279ee3a28e06ad8ae67. Consulter BASE_GITHUB.md pour retrouver
chaque pilote et sa fonction. Les fonctions Grove et les types CESI fournis sont
conservés ; les fichiers de façade dans src ne remplacent pas les pilotes.
