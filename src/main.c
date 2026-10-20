// Rôle : Démarre la carte puis appelle continuellement la supervision de la station.
// Point d'entrée du programme embarqué : setup.c prépare la carte, puis app.c pilote la station.
// Les pilotes Grove sont utilisés à travers les services déclarés dans services.h.
// Il n'y a pas de système de tâches ici : la boucle rappelle régulièrement l'application.
#include <stdio.h>
#include "services.h"
#include "board.h"
#include "setup.h"
#include "cesi_types.h"
#include "grove_rgb.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_rtc_ds1307.h"
#include "grove_gps_air530z.h"
#include "grove_bme680.h"
#include "sd_logger.h"

// Appelée au démarrage après la préparation de la mémoire par le code de démarrage STM32.
// Ne renvoie jamais : chaque passage fait avancer les traitements en cours.
int main(void) {
    // Le matériel doit être prêt avant de charger les réglages et d'interroger les capteurs.
    Global_Init();
    App_Init();

    while (1) {
        // HAL_GetTick fournit des millisecondes depuis le démarrage, pas la date de la RTC.
        App_Process(HAL_GetTick());
    }
}