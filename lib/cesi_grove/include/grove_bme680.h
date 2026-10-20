// Interface héritée BME680, conservée mais absente du chemin d'acquisition de la maquette BME280.
// Init/ReadChipId vérifient l'identification I2C ; ReadEnvironment renvoie actuellement faux.
#ifndef GROVE_BME680_H
#define GROVE_BME680_H

#include <stdbool.h>
#include "cesi_types.h"

bool GroveBME680_Init(void);
bool GroveBME680_ReadChipId(uint8_t *chip_id);
bool GroveBME680_ReadEnvironment(Env_Data *data);

#endif
