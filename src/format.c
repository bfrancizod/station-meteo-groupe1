// Rôle : Transforme une acquisition en ligne de texte pour la carte SD et la console.
// Sérialisation des mesures : contrat partagé entre storage.c, app.c et le parseur Python.
// Huit champs séparés par ';' ; l'ordre et les libellés doivent rester compatibles.
/* Format partagé console/fichier : aucun nombre invalide présenté comme mesure. */
#include "services.h"
#include <stdio.h>
#include <string.h>
// Lit m sans la modifier et écrit dans le tampon out de size octets, terminateur compris.
// Renvoie la longueur utile (avec '\n', sans '\0'), ou -1 si le tampon est trop petit.
int Measurement_Format(const Measurement *m, char *out, unsigned size) {
    // NA représente ici toutes les valeurs non OK : absentes, désactivées, ignorées
    // volontairement ou incohérentes. Le texte seul ne permet pas de distinguer ces causes.
    char stamp[32] = "NA", t[24] = "NA", h[24] = "NA", p[24] = "NA", l[16] = "NA", lat[24] = "NA",
         lon[24] = "NA";
    if (m->date_valid)
        snprintf(stamp, sizeof(stamp), "%04u-%02u-%02uT%02u:%02u:%02u", m->date.year, m->date.month,
                 m->date.day, m->date.hour, m->date.minute, m->date.second);
    if (m->status[SENSOR_TEMP] == VALUE_OK)
        snprintf(t, sizeof(t), "%.2f", (double)m->temperature);
    if (m->status[SENSOR_HUM] == VALUE_OK)
        snprintf(h, sizeof(h), "%.2f", (double)m->humidity);
    if (m->status[SENSOR_PRESS] == VALUE_OK)
        snprintf(p, sizeof(p), "%.2f", (double)m->pressure);
    if (m->status[SENSOR_LIGHT] == VALUE_OK)
        snprintf(l, sizeof(l), "%u", m->light);
    if (m->status[SENSOR_GPS] == VALUE_OK) {
        snprintf(lat, sizeof(lat), "%.6f", m->gps.lat);
        snprintf(lon, sizeof(lon), "%.6f", m->gps.lon);
    }
    const char *mode = m->mode == MODE_STANDARD   ? "STANDARD"
                       : m->mode == MODE_ECONOMIC ? "ECONOMIQUE"
                                                  : "MAINTENANCE";
    // snprintf borne l'écriture mais renvoie la longueur qui aurait été nécessaire ;
    // n >= size indique donc une troncature, même si une chaîne partielle a été écrite.
    int n = snprintf(out, size, "%s;%s;%s;%s;%s;%s;%s;%s\n", stamp, mode, t, h, p, l, lat, lon);
    return n < 0 || (unsigned)n >= size ? -1 : n;
}
