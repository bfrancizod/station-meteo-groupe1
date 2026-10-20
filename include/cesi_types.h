// Rôle : Définit les structures de données échangées avec les pilotes Grove.
// Structures de la base pédagogique échangées par les pilotes Grove.
// Les services src/time_service.c, gps.c et environment.c les adaptent aux types applicatifs.
#ifndef CESI_TYPES_H
#define CESI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

// Heure lue/écrite dans la DS1307 : année complète, jour de semaine de dimanche=1 à samedi=7.
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t dayOfWeek;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} RTC_DateTime;

// Résultat NMEA : valid qualifie la position ; raw_sentence est une copie bornée de la phrase.
// Latitude/longitude sont en degrés décimaux ; qualité et satellites viennent de la GGA.
typedef struct
{
    bool valid;
    char raw_sentence[96];

    double latitude;
    double longitude;

    uint8_t fix_quality;
    uint8_t satellites;
} GPS_Data;

// Mesures environnementales avec unités explicites. Sur BME280, la résistance gaz vaut NaN :
// cette grandeur n'est pas mesurée ; temperature/humidity/pressure restent indépendantes.
typedef struct
{
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
    float gas_resistance_ohm;
} Env_Data;

#endif
