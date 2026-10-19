# Vérifications de WWW-G1-1.1.0 intégrée à la base officielle

Date : 18 octobre 2026.

## Base et interfaces

Dépôt cloné : https://github.com/ValentinMoraine/Worldwide-Weather-Watcher.git
Commit : 3c8fff4c410b22ee1b4ad279ee3a28e06ad8ae67.
Comparaison avec l'archive initialement fournie : mêmes fichiers.
Tous les fichiers suivis d'origine sont présents ; les empreintes et états sont
consignés dans UPSTREAM_MANIFEST.json.

Le code passe par les interfaces Grove conservées ou complétées. En particulier,
l'initialisation SD originale et la fonction SDLogger_WriteLine sont utilisées.
Le pilote BME680 d'origine reste distinct du nouveau pilote BME280 réellement actif.

## Compilation ARM exécutée

- Compilateur xPack GCC ARM 15.2.1, Cortex-M4F, ABI hard-float.
- Sources officielles ST : HAL STM32L4 v1.13.5, CMSIS Device L4 v1.7.4.
- En-têtes ARM CMSIS Core 5.9.0.
- Compilation C GNU11 Os et édition de liens réalisées directement, sans appeler
  le client PlatformIO ni un service distant.
- Configuration HAL propre au projet, pile 8 Ko et tas de conversion borné 4 Ko.
- Rapport final du compilateur et du linker sans avertissement ni erreur.

```text
text = 64 232 octets
data =    524 octets
bss  = 21 544 octets, réserves de pile et tas comprises
```

Flash : 64 756 octets sur 1 044 480 octets autorisés. Les 4 096 derniers octets
sont réservés à la configuration. SRAM1 allouée et réservée : 22 068 octets sur
98 304 octets. SRAM2 non allouée implicitement. Ces valeurs sont issues du linker,
pas d'une mesure de la marge de pile sur la carte. Le script conserve le chargement
des données initialisées en RAM et laisse Reset_Handler mettre la BSS à zéro.

## Tests PC exécutés

Les trois suites passent avec AddressSanitizer et UndefinedBehaviorSanitizer,
détection des fuites désactivée dans le conteneur :

1. Configuration, limites numériques, CRC, calendrier, NMEA GGA, coordonnées,
   checksum, champs vides et format des mesures. Le parseur testé est celui du
   pilote Grove réintégré, utilisé via sa façade applicative.
2. Vrai FatFs sur disque RAM : création et rotation, révision 10 et suivantes,
   remontage, refus d'une archive temporaire existante sans altérer _0, date invalide.
   Les lignes passent par la vraie fonction SDLogger_WriteLine ajoutée au pilote.
3. Superviseur avec périphériques simulés : mesure, maintenance, libération SD,
   retour au standard, économie et arbitrage A1 du bouton rouge.

## Limites de cette validation

Aucun accès à la maquette physique n'a été effectué. Restent à vérifier :
alimentations, broches, polarité des boutons, signaux SPI, fix GPS, mesures réelles,
conservation Flash après coupure, latence maximale, consommation et marge de pile.
Le chemin de compilation PlatformIO complet n'a pas été exécuté ici ; son projet
est fourni pour l'utilisation dans VS Code. Le binaire livré provient de la
compilation directe décrite ci-dessus.

Les capteurs tiers non fournis ne sont pas simulés comme s'ils étaient présents.
L'arbitrage du sujet sur le rouge en économie doit être confirmé avec le tuteur,
et la couverture des prosits non communiqués ne peut pas être attestée.
