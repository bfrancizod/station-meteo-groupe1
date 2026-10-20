// Interface du pilote adapté BME280, utilisée par src/environment.c.
// Init configure le capteur via I2C ; Begin déclenche ; Read copie les mesures compensées dans data.
#ifndef GROVE_BME280_H
#define GROVE_BME280_H
#include <stdbool.h>
#include "cesi_types.h"
/* Complément matériel de la base : BME280 VMA335, pas BME680. */
bool GroveBME280_Init(void);
bool GroveBME280_Begin(void);
// data doit désigner un Env_Data ; vrai rend ses °C, % et hPa utilisables, gaz non disponible.
bool GroveBME280_ReadEnvironment(Env_Data *data);
#endif
