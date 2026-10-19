#ifndef GROVE_GPS_AIR530Z_H
#define GROVE_GPS_AIR530Z_H

#include <stdbool.h>
#include "cesi_types.h"

void GroveGPS_Init(void);
bool GroveGPS_ReadLine(char *buffer, unsigned int max_len, unsigned int timeout_ms);
bool GroveGPS_ReadGGA(GPS_Data *gps, unsigned int timeout_ms);

/* Extensions non bloquantes communes Air530Z / SIM28 (NMEA GGA). */
void GroveGPS_Process(void);
void GroveGPS_Reset(void);
bool GroveGPS_Take(uint32_t since,GPS_Data *out);
bool GroveGPS_Parse(const char *line,GPS_Data *out);
#endif
