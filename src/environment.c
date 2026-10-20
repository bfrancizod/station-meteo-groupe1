// Rôle : Transmet à l'application les mesures de température, d'humidité et de pression.
// Pont entre app.c (trois valeurs séparées) et grove_bme280.c (structure Env_Data).
// La compensation physique est assurée par le pilote Bosch derrière l'interface Grove.
/* Adaptation applicative des interfaces Grove : les types fournis sont réutilisés. */
#include "services.h"
#include "grove_bme280.h"
// Détecte et configure le BME280 ; renvoie faux s'il est absent ou si l'échange échoue.
bool Env_Init(void) {
    return GroveBME280_Init();
}
// Déclenche une conversion forcée ; vrai signifie démarrage accepté, pas mesure déjà prête.
bool Env_Begin(void) {
    return GroveBME280_Begin();
}
// Lit après le délai laissé par app.c ; t/h/p sont des adresses de sortie valides.
// Renvoie les °C, le pourcentage d'humidité et les hPa ; faux ne modifie pas ces sorties.
bool Env_Read(float *t, float *h, float *p) {
    Env_Data data;
    if (!GroveBME280_ReadEnvironment(&data))
        return false;
    // *t écrit dans la variable de l'appelant ; data est seulement une copie locale.
    *t = data.temperature_c;
    *h = data.humidity_percent;
    *p = data.pressure_hpa;
    return true;
}
