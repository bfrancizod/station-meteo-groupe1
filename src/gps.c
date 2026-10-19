/* Façade applicative du pilote NMEA fourni et complété. */
#include "services.h"
#include "grove_gps_air530z.h"
static void convert(const GPS_Data *in, GpsFix *out) {
    *out = (GpsFix){in->latitude, in->longitude, in->fix_quality, in->satellites};
}
bool Gps_Parse(const char *line, GpsFix *out) {
    GPS_Data data;
    if (!out || !GroveGPS_Parse(line, &data))
        return false;
    convert(&data, out);
    return true;
}
#ifndef HOST_TEST
void Gps_Process(void) {
    GroveGPS_Process();
}
void Gps_Reset(void) {
    GroveGPS_Reset();
}
bool Gps_Take(uint32_t since, GpsFix *out) {
    GPS_Data data;
    if (!GroveGPS_Take(since, &data))
        return false;
    convert(&data, out);
    return true;
}
#endif
