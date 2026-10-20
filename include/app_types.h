// Rôle : Définit les types partagés pour les modes, les réglages, les mesures et les erreurs.
// Types communs de l'application, sans dépendance matérielle : app.c, services et tests PC.
// Une structure regroupe les champs d'un objet ; une énumération donne des noms à des valeurs entières.
#ifndef APP_TYPES_H
#define APP_TYPES_H
#include <stdbool.h>
#include <stdint.h>
// L'ordre sert aussi à indexer les noms dans app.c ; chaque valeur identifie un mode.
typedef enum { MODE_STANDARD, MODE_CONFIGURATION, MODE_MAINTENANCE, MODE_ECONOMIC } AppMode;
// OK = exploitable, DISABLED = désactivé, SKIPPED = ignoré volontairement (GPS économique),
// TIMEOUT = attente non satisfaite, IO_ERROR = erreur d'accès, INVALID = mesure incohérente.
// IO_ERROR existe dans le type mais n'est pas affecté par le cycle actuel d'app.c.
typedef enum {
    VALUE_OK,
    VALUE_DISABLED,
    VALUE_SKIPPED,
    VALUE_TIMEOUT,
    VALUE_IO_ERROR,
    VALUE_INVALID
} ValueStatus;
// Ces indices correspondent aux cases status/timeout_count et aux bits d'erreur par capteur.
// SENSOR_COUNT est le nombre de capteurs logiques, pas un capteur supplémentaire.
typedef enum {
    SENSOR_LIGHT,
    SENSOR_TEMP,
    SENSOR_HUM,
    SENSOR_PRESS,
    SENSOR_GPS,
    SENSOR_COUNT
} SensorId;
// Calendrier : année complète (2000..2099), mois/jour à partir de 1, weekday dimanche=1.
// uint8_t/uint16_t/uint32_t sont des entiers non signés sur 8/16/32 bits, de taille explicite.
typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
} DateTime;
// Coordonnées en degrés décimaux, signées (sud/ouest négatifs),
// avec les informations sur la validité de la position GGA.
typedef struct {
    double lat, lon;
    uint8_t quality, satellites;
} GpsFix;
// Réglages persistés en Flash : interval_min en minutes, timeout_s en secondes,
// file_max en octets ; activations 0/1 ; luminosité 0..1023 ; seuils en °C et hPa.
// Les seuils lumin_low/lumin_high sont configurables ; le cycle publie la valeur brute réduite.
typedef struct {
    uint16_t interval_min, timeout_s;
    uint32_t file_max;
    uint8_t lumin, temp, hygr, pressure;
    uint16_t lumin_low, lumin_high;
    float temp_min, temp_max, hygr_mint, hygr_maxt, pressure_min, pressure_max;
} Config;
// Une acquisition complète : date RTC, mode au départ, mesures et validité de chaque source.
// Les champs numériques seuls ne disent pas si une lecture a réussi : consulter status.
// Température en °C, humidité en %, pression en hPa ; date_valid qualifie séparément la RTC.
typedef struct {
    DateTime date;
    bool date_valid;
    AppMode mode;
    float temperature, humidity, pressure;
    uint16_t light;
    GpsFix gps;
    ValueStatus status[SENSOR_COUNT];
} Measurement;
// Puissances de deux : | regroupe les défauts, & teste un défaut, & ~masque le retire.
/* Un bit par défaut. Les capteurs conservent leurs défauts individuellement. */
enum {
    ERR_RTC = 1,
    ERR_GPS = 2,
    ERR_SENSOR_ACCESS = 4,
    ERR_SENSOR_INVALID = 8,
    ERR_SD_FULL = 16,
    ERR_SD_IO = 32
};
#endif
