/* Tests exécutables sur PC : aucun matériel STM32 nécessaire. */
#include "services.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void nmea(const char *payload, char *out) {
    unsigned c = 0;
    for (const char *p = payload; *p; p++)
        c ^= (unsigned char)*p;
    sprintf(out, "$%s*%02X", payload, c);
}
int main(void) {
    Config c;
    Config_Defaults(&c);
    assert(Config_Validate(&c));
    assert(!Config_Set(&c, "LOG_INTERVAL", "0"));
    assert(!Config_Set(&c, "LOG_INTERVAL", "65537"));
    assert(!Config_Set(&c, "LUMIN", "256"));
    assert(!Config_Set(&c, "TIMEOUT", "NaN"));
    assert(!Config_Set(&c, "FILE_MAX_SIZE", "2e99"));
    assert(!Config_Set(&c, "PRESSURE_MIN", "2000"));
    assert(!Config_Set(&c, "LUMIN_LOW", "900"));
    assert(Config_Set(&c, "LOG_INTERVALL", "1"));
    assert(c.interval_min == 1);
    assert(Config_Set(&c, "MIN_TEMP_AIR", "-20.5"));
    assert(c.temp_min == -20.5);
    assert(Crc32("123456789", 9) == 0xCBF43926);
    DateTime d = {2024, 2, 29, 12, 30, 0, 5};
    assert(Time_Valid(&d));
    d.year = 2023;
    assert(!Time_Valid(&d));
    d.year = 2024;
    d.second = 60;
    assert(!Time_Valid(&d));
    d = (DateTime){2026, 10, 16, 0, 0, 0, 6};
    assert(Time_Weekday(&d) == 6);
    char line[128];
    GpsFix g;
    assert(Gps_Parse("$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47", &g));
    assert(fabs(g.lat - 48.1173) < 1e-6);
    assert(fabs(g.lon - 11.5166667) < 1e-6);
    nmea("GNGGA,123519,0000.000,N,00000.000,E,1,08,1,0,M,0,M,,", line);
    assert(Gps_Parse(line, &g) && g.lat == 0 && g.lon == 0);
    nmea("GPGGA,123519,,,,,0,00,99.9,,,,,,", line);
    assert(!Gps_Parse(line, &g));
    nmea("GPGGA,123519,4860.000,N,01131.000,E,1,08,1,0,M,0,M,,", line);
    assert(!Gps_Parse(line, &g));
    nmea("GPGGA,123519,4807.000,X,01131.000,E,1,08,1,0,M,0,M,,", line);
    assert(!Gps_Parse(line, &g));
    nmea("GPGGA,123519,4807.000,S,01131.000,W,1,08,1,0,M,0,M,,", line);
    assert(Gps_Parse(line, &g) && g.lat < 0 && g.lon < 0);
    line[10] ^= 1;
    assert(!Gps_Parse(line, &g));
    Measurement m = {0};
    for (unsigned i = 0; i < SENSOR_COUNT; i++)
        m.status[i] = VALUE_DISABLED;
    m.mode = MODE_STANDARD;
    m.light = 0;
    m.status[SENSOR_LIGHT] = VALUE_OK;
    char record[256];
    assert(Measurement_Format(&m, record, sizeof(record)) > 0);
    assert(!strcmp(record, "NA;STANDARD;NA;NA;NA;0;NA;NA\n"));
    assert(Measurement_Format(&m, record, 5) < 0);
    nmea("GPGGA,123519,1e300,N,01131.000,E,1,08,1,0,M,0,M,,", line);
    assert(!Gps_Parse(line, &g));
    /* Compteurs uint32_t : les comparaisons restent valides au débordement. */
    uint32_t start = 0xfffffff0u, now = 0x20;
    assert(now - start == 48);
    puts("Tests configuration, CRC, calendrier, NMEA et format : OK");
    return 0;
}
