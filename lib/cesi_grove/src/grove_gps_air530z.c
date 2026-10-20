// Rôle : Reconstitue les phrases GPS reçues et valide les positions GGA récentes.
// Pilote NMEA utilisé pour le SIM28 malgré son nom historique Air530Z.
// setup.c reçoit les octets en interruption ; ce fichier assemble et valide les GGA,
// puis src/gps.c transmet les positions à l'acquisition. Une RMC seule n'est pas exploitée.
// Le diagnostic reste réactivable par cette constante ; aucune commande n'est envoyée au GPS.
#define GPS_DIAG_ENABLED 0
/* NMEA GGA : somme de contrôle, champs vides conservés et coordonnées vérifiées.
 * Compatible GP/GN et autres identifiants d'émetteur ; aucune position périmée n'est publiée. */
#include "grove_gps_air530z.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifndef HOST_TEST
#include "board.h"
// line reconstitue une phrase ; used est le nombre de caractères déjà reçus.
// last garde la dernière GGA valide et last_ms sa réception locale, pas l'heure GPS.
static char line[128];
static unsigned used;
static uint32_t last_ms;
static bool available;
static GPS_Data last;
static char recent_line[128];
static uint32_t line_sequence;
#endif
// Convertit un chiffre hexadécimal de la somme de contrôle en 0..15 ; -1 signale un caractère invalide.
static int hex(char c) {
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return -1;
}
// Convertit toute la chaîne s en nombre fini dans *out ; faux interdit d'utiliser le résultat.
static bool number(const char *s, double *out) {
    char *end;
    *out = strtod(s, &end);
    return *s && !*end && isfinite(*out);
}
// s est en degrés/minutes NMEA (ddmm.mmmm ou dddmm.mmmm), hem indique N/S/E/W.
// lat choisit les bornes de latitude ; vrai écrit les degrés décimaux dans *out.
// Exemple : 4807.038 N devient 48 + 7.038/60, soit 48.1173 degrés.
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
// s doit contenir une phrase sans CR/LF, de '$' jusqu'aux deux chiffres après '*'.
// Fonction indépendante du matériel, appelée dans la boucle et dans les tests PC.
// Remplit *out uniquement si la somme de contrôle, le type GGA, la qualité et les coordonnées sont acceptés.
bool GroveGPS_Parse(const char *s, GPS_Data *out) {
    if (!s || !out || *s != '$' || strlen(s) >= 128)
        return false;
    const char *star = strchr(s, '*');
    if (!star || strlen(star) != 3)
        return false;
    int a = hex(star[1]), b = hex(star[2]);
    if (a < 0 || b < 0)
        return false;
    // La somme de contrôle NMEA est un XOR des octets entre '$' et '*', exclus.
    // a << 4 place le premier chiffre hexadécimal dans les quatre bits forts.
    uint8_t crc = 0;
    for (const char *p = s + 1; p < star; p++)
        crc ^= (uint8_t)*p;
    if (crc != (uint8_t)((a << 4) | b))
        return false;
    char copy[128];
    size_t len = star - s - 1;
    memcpy(copy, s + 1, len);
    copy[len] = 0;
    // Copie locale modifiable : les pointeurs f désignent les champs de cette copie.
    // Remplacer chaque virgule conserve un champ vide entre deux virgules successives,
    // indispensable pour ne pas décaler latitude, longitude, qualité et satellites.
    char *f[20];
    unsigned count = 1;
    f[0] = copy;
    for (char *p = copy; *p; p++)
        if (*p == ',') {
            *p = 0;
            if (count < 20)
                f[count++] = p + 1;
        }
    // Les deux premiers caractères identifient l'émetteur (GP, GN...) ; seuls GGA comptent.
    // Qualité 0 = absence de position valide, refusée ici ; le nombre de satellites est contrôlé séparément.
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
    // La phrase brute peut être raccourcie dans raw_sentence sans modifier les coordonnées.
    size_t raw_len = strlen(s);
    if (raw_len >= sizeof(g.raw_sentence))
        raw_len = sizeof(g.raw_sentence) - 1;
    memcpy(g.raw_sentence, s, raw_len);
    g.raw_sentence[raw_len] = 0;
    *out = g;
    return true;
}
#ifndef HOST_TEST
// Appelée au début d'une acquisition : invalide le cache et vide les octets déjà reçus.
// Le récepteur continue à fonctionner ; les prochains octets pourront former une nouvelle GGA.
void GroveGPS_Reset(void) {
    available = false;
    used = 0;
    while (Board_GpsGet() >= 0) {
    }
}
// Consomme seulement les octets disponibles ; n'attend pas une position satellite.
// Appelée hors interruption via App_Background ; le compteur de diagnostic compte aussi
// les phrases non GGA et les lignes invalides, il ne compte donc pas uniquement les positions valides.
void GroveGPS_Process(void) {
    /* Diagnostic GPS temporaire */
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
            // Un nouveau '$' resynchronise le début ; '\n' termine et '\r' est ignoré.
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
                // Une phrase trop longue est abandonnée ; la prochaine '$' pourra repartir.
                used = 0;
        }
    }
}
// since est l'instant de début d'acquisition en ms HAL ; out doit être valide.
// Refuse une GGA antérieure au cycle ou reçue il y a plus de 2 s. La comparaison signée
// suppose des instants séparés de moins de 2^31 ms ; elle tolère le rebouclage du compteur.
// La lecture ne consomme pas le cache : plusieurs appels peuvent renvoyer la même position.
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
// Configure UART4 à 9600 bauds et applique GPS_SWAP_PINS ; setup.c configure les broches/IRQ.
// La permutation actuelle échange RX et TX sur PA0/PA1 ; elle reste définie dans board_pins.h.
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
// Interface bloquante de compatibilité : attend une nouvelle ligne au plus timeout_ms.
// max_len inclut le '\0' ; vrai signifie ligne copiée, sans garantir qu'elle est une GGA valide.
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
// Interface bloquante de compatibilité : cherche une GGA récente jusqu'à l'expiration du délai en ms.
// Sert le fond pendant l'attente ; le cycle normal utilise Process/Take à la place.
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
