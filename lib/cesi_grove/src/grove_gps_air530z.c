#define GPS_DIAG_ENABLED 0
/* NMEA GGA : checksum, champs vides conservés et coordonnées vérifiées.
 * Compatible GP/GN et autres talkers ; aucune position périmée n'est publiée. */
#include "grove_gps_air530z.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef HOST_TEST
#include "board.h"
static char line[128];
static unsigned used;
static uint32_t last_ms;
static bool available;
static GPS_Data last;
static char recent_line[128];
static uint32_t line_sequence;
#endif
static int hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}
static bool number(const char *s, double *out) {
    char *end;
    *out = strtod(s, &end);
    return *s && !*end && isfinite(*out);
}
static bool coordinate(const char *s, const char *hem, bool lat, double *out) {
    double raw;
    if (!number(s, &raw) || raw < 0 || raw > 18060 || strlen(hem) != 1)
        return false;
    if (lat ? (*hem != 'N' && *hem != 'S') : (*hem != 'E' && *hem != 'W'))
        return false;
    unsigned deg = (unsigned)(raw / 100);
    double min = raw - deg * 100;
    if (min >= 60 || deg > (lat ? 90u : 180u) || (deg == (lat ? 90u : 180u) && min > 0))
        return false;
    *out = deg + min / 60;
    if (*hem == 'S' || *hem == 'W')
        *out = -*out;
    return true;
}
/* Valider toute la trame avant de toucher au résultat ; conserver les champs vides NMEA. */
bool GroveGPS_Parse(const char *s, GPS_Data *out) {
    if (!s || !out || *s != '$' || strlen(s) >= 128)
        return false;
    const char *star = strchr(s, '*');
    if (!star || strlen(star) != 3)
        return false;
    int a = hex(star[1]), b = hex(star[2]);
    if (a < 0 || b < 0)
        return false;
    uint8_t crc = 0;
    for (const char *p = s + 1; p < star; p++)
        crc ^= (uint8_t)*p;
    if (crc != (uint8_t)((a << 4) | b))
        return false;
    char copy[128];
    size_t len = star - s - 1;
    memcpy(copy, s + 1, len);
    copy[len] = 0;
    char *f[20];
    unsigned count = 1;
    f[0] = copy;
    for (char *p = copy; *p; p++)
        if (*p == ',') {
            *p = 0;
            if (count < 20)
                f[count++] = p + 1;
        }
    if (count < 8 || strlen(f[0]) != 5 || strcmp(f[0] + 2, "GGA"))
        return false;
    double quality, sats;
    if (!number(f[6], &quality) || quality < 1 || quality > 8 || floor(quality) != quality ||
        !number(f[7], &sats) || sats < 0 || sats > 99 || floor(sats) != sats)
        return false;
    GPS_Data g = {0};
    if (!coordinate(f[2], f[3], true, &g.latitude) || !coordinate(f[4], f[5], false, &g.longitude))
        return false;
    g.fix_quality = quality;
    g.satellites = sats;
    g.valid = true;
    size_t raw_len = strlen(s);
    if (raw_len >= sizeof(g.raw_sentence))
        raw_len = sizeof(g.raw_sentence) - 1;
    memcpy(g.raw_sentence, s, raw_len);
    g.raw_sentence[raw_len] = 0;
    *out = g;
    return true;
}
#ifndef HOST_TEST
void GroveGPS_Reset(void) {
    available = false;
    used = 0;
    while (Board_GpsGet() >= 0) {
    }
}
void GroveGPS_Process(void) {
    /* GPS_DIAG_TEMP */
    static uint32_t diag_bytes, diag_tick;
    uint32_t now = HAL_GetTick();
    if (GPS_DIAG_ENABLED && now - diag_tick >= 5000) {
        diag_tick = now;
        printf("[GPS diag] octets=%lu phrases=%lu derniere=%.100s\r\n",
               (unsigned long)diag_bytes,
               (unsigned long)line_sequence,
               recent_line[0] ? recent_line : "(aucune)");
    }
    int c;
    while ((c = Board_GpsGet()) >= 0) {
        diag_bytes++;
        if (c == '$') {
            used = 0;
            line[used++] = '$';
        } else if (c == '\n') {
            line[used] = 0;
            strncpy(recent_line, line, sizeof(recent_line));
            line_sequence++;
            GPS_Data g;
            if (GroveGPS_Parse(line, &g)) {
                last = g;
                last_ms = HAL_GetTick();
                available = true;
            }
            used = 0;
        } else if (c != '\r' && used) {
            if (used < sizeof(line) - 1)
                line[used++] = (char)c;
            else
                used = 0;
        }
    }
}
bool GroveGPS_Take(uint32_t since, GPS_Data *out) {
    if (!available || (int32_t)(last_ms - since) < 0 || HAL_GetTick() - last_ms > 2000)
        return false;
    *out = last;
    return true;
}
#endif

#ifndef HOST_TEST
/* Initialisation et signatures héritées du dépôt. La voie asynchrone est utilisée
 * par l'application ; les appels bloquants restent des outils de compatibilité. */
void GroveGPS_Init(void) {
    extern UART_HandleTypeDef GPS_UART_HANDLE;
    GPS_UART_HANDLE.Instance = UART4;
    GPS_UART_HANDLE.Init.BaudRate = 9600;
    GPS_UART_HANDLE.Init.WordLength = UART_WORDLENGTH_8B;
    GPS_UART_HANDLE.Init.StopBits = UART_STOPBITS_1;
    GPS_UART_HANDLE.Init.Parity = UART_PARITY_NONE;
    GPS_UART_HANDLE.Init.Mode = UART_MODE_TX_RX;
    GPS_UART_HANDLE.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    GPS_UART_HANDLE.Init.OverSampling = UART_OVERSAMPLING_16;
    GPS_UART_HANDLE.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
    GPS_UART_HANDLE.AdvancedInit.Swap =
        GPS_SWAP_PINS ? UART_ADVFEATURE_SWAP_ENABLE : UART_ADVFEATURE_SWAP_DISABLE;
    HAL_UART_Init(&GPS_UART_HANDLE);
}
bool GroveGPS_ReadLine(char *out, unsigned max_len, unsigned timeout_ms) {
    if (!out || !max_len)
        return false;
    uint32_t seq = line_sequence, start = HAL_GetTick();
    do {
        GroveGPS_Process();
        if (line_sequence != seq) {
            if (strlen(recent_line) >= max_len) {
                out[0] = 0;
                return false;
            }
            strcpy(out, recent_line);
            return true;
        }
        Board_Pump();
    } while (HAL_GetTick() - start < timeout_ms);
    out[0] = 0;
    return false;
}
bool GroveGPS_ReadGGA(GPS_Data *out, unsigned timeout_ms) {
    if (!out)
        return false;
    uint32_t start = HAL_GetTick();
    do {
        GroveGPS_Process();
        if (GroveGPS_Take(start, out))
            return true;
        Board_Pump();
    } while (HAL_GetTick() - start < timeout_ms);
    return false;
}
#endif
