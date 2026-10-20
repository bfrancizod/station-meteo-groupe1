// Interface I2C RTC, traduite par src/time_service.c vers les types applicatifs.
// Init teste la présence ; GetDateTime exige une horloge active et une date valide.
// SetDateTime écrit une date validée ; les booléens indiquent le succès, dt appartient à l'appelant.
#ifndef GROVE_RTC_DS1307_H
#define GROVE_RTC_DS1307_H

#include <stdbool.h>
#include "cesi_types.h"

bool GroveRTC_Init(void);
bool GroveRTC_GetDateTime(RTC_DateTime *dt);
bool GroveRTC_SetDateTime(const RTC_DateTime *dt);

#endif
