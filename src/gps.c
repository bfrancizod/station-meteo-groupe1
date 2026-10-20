// Adaptateur entre le GPS_Data de la base Grove et le GpsFix utilisé par Measurement.
// app.c appelle ces services ; l'assemblage des phrases et leur validation restent au pilote.
/* Façade applicative du pilote NMEA fourni et complété. */
#include "services.h"
#include "grove_gps_air530z.h"
// Copie position en degrés décimaux, qualité et satellites ; les pointeurs doivent être valides.
static void convert(const GPS_Data *in, GpsFix *out) {
    *out = (GpsFix){in->latitude, in->longitude, in->fix_quality, in->satellites};
}
// Analyse une chaîne GGA complète sans UART ; faux ne publie aucune position dans *out.
// Cette voie purement textuelle est aussi utilisée dans les tests sur PC.
bool Gps_Parse(const char *line, GpsFix *out) {
    GPS_Data data;
    if (!out || !GroveGPS_Parse(line, &data))
        return false;
    convert(&data, out);
    return true;
}
// HOST_TEST retire les fonctions matérielles ; les tests peuvent fournir leurs doublures.
#ifndef HOST_TEST
// Consomme les octets UART disponibles, sans attendre une nouvelle phrase.
void Gps_Process(void) {
    GroveGPS_Process();
}
// Invalide la position précédente et vide la réception en attente au début d'un cycle.
void Gps_Reset(void) {
    GroveGPS_Reset();
}
// since est le début de collecte en ms HAL ; vrai remplit *out avec une position récente.
// L'appelant fournit un pointeur valide ; une absence de fix laisse la mesure en attente.
bool Gps_Take(uint32_t since, GpsFix *out) {
    GPS_Data data;
    if (!GroveGPS_Take(since, &data))
        return false;
    convert(&data, out);
    return true;
}
#endif
