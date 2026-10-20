// Interface historique Air530Z utilisée pour le SIM28 et adaptée par src/gps.c.
// Les données GPS_Data sont validées depuis des GGA ; le seul trafic UART ne garantit pas un fix.
#ifndef GROVE_GPS_AIR530Z_H
#define GROVE_GPS_AIR530Z_H

#include <stdbool.h>
#include "cesi_types.h"

// Init règle UART4 ; ReadLine/ReadGGA attendent au plus timeout_ms dans la boucle appelante.
// max_len inclut le terminateur ; une ligne copiée par ReadLine peut ne pas être une GGA valide.
void GroveGPS_Init(void);
bool GroveGPS_ReadLine(char *buffer, unsigned int max_len, unsigned int timeout_ms);
bool GroveGPS_ReadGGA(GPS_Data *gps, unsigned int timeout_ms);

// Process consomme le flux disponible ; Reset purge le cache et la réception.
// Take remplit out si la GGA date d'au moins since (ms HAL) et d'au plus 2 s.
// Parse attend '$...*XX' sans CR/LF et ne remplit out qu'en cas de succès.
/* Extensions non bloquantes communes Air530Z / SIM28 (NMEA GGA). */
void GroveGPS_Process(void);
void GroveGPS_Reset(void);
bool GroveGPS_Take(uint32_t since,GPS_Data *out);
bool GroveGPS_Parse(const char *line,GPS_Data *out);
#endif
