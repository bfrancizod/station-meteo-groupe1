#ifndef GROVE_BME280_H
#define GROVE_BME280_H
#include <stdbool.h>
#include "cesi_types.h"
/* Complément matériel de la base : BME280 VMA335, pas BME680. */
bool GroveBME280_Init(void);
bool GroveBME280_Begin(void);
bool GroveBME280_ReadEnvironment(Env_Data *data);
#endif
