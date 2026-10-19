#include "services.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
void Config_Defaults(Config *c) {
    memset(c, 0, sizeof(*c));
    c->interval_min = 10;
    c->timeout_s = 30;
    c->file_max = 2048;
    c->lumin = c->temp = c->hygr = c->pressure = 1;
    c->lumin_low = 255;
    c->lumin_high = 768;
    c->temp_min = -10;
    c->temp_max = 60;
    c->hygr_mint = 0;
    c->hygr_maxt = 50;
    c->pressure_min = 850;
    c->pressure_max = 1080;
}
bool Config_Validate(const Config *c) {
    return c->interval_min >= 1 && c->interval_min <= 1440 && c->timeout_s >= 1 &&
           c->timeout_s <= 120 && c->file_max >= 512 && c->file_max <= 1048576 && c->lumin <= 1 &&
           c->temp <= 1 && c->hygr <= 1 && c->pressure <= 1 && c->lumin_low < c->lumin_high &&
           c->lumin_high <= 1023 && isfinite(c->temp_min) && isfinite(c->temp_max) &&
           c->temp_min >= -40 && c->temp_min < c->temp_max && c->temp_max <= 85 &&
           isfinite(c->hygr_mint) && isfinite(c->hygr_maxt) && c->hygr_mint >= -40 &&
           c->hygr_mint < c->hygr_maxt && c->hygr_maxt <= 85 && isfinite(c->pressure_min) &&
           isfinite(c->pressure_max) && c->pressure_min >= 300 &&
           c->pressure_min < c->pressure_max && c->pressure_max <= 1100;
}
bool Config_Set(Config *c, const char *key, const char *value) {
    char *end;
    double v = strtod(value, &end);
    if (!*value || *end || !isfinite(v))
        return false;
    Config n = *c;
/* Comparer avant le cast : aucun débordement d'entier accepté. */
#define UINT_FIELD(name, field, lo, hi)                                                            \
    if (!strcmp(key, name)) {                                                                      \
        if (v < (lo) || v > (hi) || floor(v) != v)                                                 \
            return false;                                                                          \
        n.field = v;                                                                               \
    }
#define FLOAT_FIELD(name, field)                                                                   \
    if (!strcmp(key, name)) {                                                                      \
        if (v < -10000 || v > 10000)                                                               \
            return false;                                                                          \
        n.field = v;                                                                               \
    }
    if (!strcmp(key, "LOG_INTERVALL"))
        key = "LOG_INTERVAL";
    UINT_FIELD("LOG_INTERVAL", interval_min, 1, 1440)
    else UINT_FIELD("TIMEOUT", timeout_s, 1, 120) else UINT_FIELD("FILE_MAX_SIZE", file_max, 512, 1048576) else UINT_FIELD(
        "LUMIN", lumin,
        0, 1) else UINT_FIELD("TEMP_AIR", temp,
                              0, 1) else UINT_FIELD("HYGR", hygr, 0,
                                                    1) else UINT_FIELD("PRESSURE", pressure, 0,
                                                                       1) else UINT_FIELD("LUMIN_"
                                                                                          "LOW",
                                                                                          lumin_low,
                                                                                          0, 1023) else UINT_FIELD("LUMIN_HIGH",
                                                                                                                   lumin_high,
                                                                                                                   0, 1023) else FLOAT_FIELD("MIN_TEMP_AIR", temp_min) else FLOAT_FIELD("MAX_TEMP_AIR", temp_max) else FLOAT_FIELD("HYGR_MINT", hygr_mint) else FLOAT_FIELD("HYGR_MAXT", hygr_maxt) else FLOAT_FIELD("PRESSURE_MIN",
                                                                                                                                                                                                                                                                                                                     pressure_min) else FLOAT_FIELD("PRESSURE_MAX",
                                                                                                                                                                                                                                                                                                                                                    pressure_max) else return false;
    if (!Config_Validate(&n))
        return false;
    *c = n;
    return true;
}
uint32_t Crc32(const void *data, uint32_t size) {
    const uint8_t *p = data;
    uint32_t c = ~0u;
    while (size--) {
        c ^= *p++;
        for (unsigned i = 0; i < 8; i++)
            c = (c >> 1) ^ ((c & 1) ? 0xEDB88320u : 0);
    }
    return ~c;
}
