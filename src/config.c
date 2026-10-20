// Paramètres de la station et contrôle de leur cohérence, sans accès au matériel.
// app.c utilise ces fonctions avant Nv_Save ; nv_store.c utilise aussi le CRC32.
#include "services.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
// Remplit la structure pointée par c : minutes, secondes, octets, °C et hPa.
// L'appelant fournit un pointeur valide ; aucune allocation ni sauvegarde n'est faite ici.
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
// Vérifie les bornes et l'ordre des seuils de toute la configuration, sans la modifier.
// isfinite rejette NaN et les infinis ; const protège les champs contre l'écriture ici.
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
// Convertit value et propose une modification du champ nommé key ; vrai si acceptée.
// Travaille sur une copie n : *c n'est remplacée que si tous les seuils restent cohérents.
// Cette fonction ne persiste rien ; app.c doit ensuite demander l'écriture Flash.
bool Config_Set(Config *c, const char *key, const char *value) {
    char *end;
    // &end permet à strtod de fournir l'adresse du premier caractère non converti.
    // Refuser *end non nul évite d'accepter silencieusement une valeur suivie de texte.
    double v = strtod(value, &end);
    if (!*value || *end || !isfinite(v))
        return false;
    Config n = *c;
// Les macros construisent la chaîne de tests ci-dessous ; leurs '\' prolongent la ligne.
// UINT_FIELD exige un entier dans les bornes avant de le ranger dans un type plus petit.
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
    // Les deux orthographes du sujet sont acceptées ; SHOW utilise LOG_INTERVAL.
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
// Calcule une empreinte de size octets à partir de data, sans modifier la zone mémoire.
// Le pointeur void est vu comme une suite d'octets ; *p++ lit l'octet puis avance.
// ^ est le OU exclusif ; les huit décalages traitent les huit bits de chaque octet.
// Ce CRC détecte des altérations accidentelles, ce n'est pas une signature de sécurité.
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
