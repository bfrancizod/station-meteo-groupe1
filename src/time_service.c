// Rôle : Valide le calendrier et relie l'application à l'horloge DS1307.
// Calendrier applicatif : traduit DateTime vers le RTC_DateTime du pilote DS1307.
// app.c l'utilise pour les commandes et l'horodatage ; diskio_spi.c pour la date FAT.
/* DS1307 : calendrier strict ; aucune date fictive injectée au démarrage. */
#include "services.h"
#ifndef HOST_TEST
#include "board.h"
#endif
// Renvoie vrai pour une année bissextile selon les règles du calendrier grégorien.
static bool leap(unsigned y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
// Vérifie date, heure et jour de semaine (1..7), sans interroger la RTC.
// Les && s'arrêtent au premier échec : le mois est contrôlé avant d'indexer days.
bool Time_Valid(const DateTime *d) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return d && d->year >= 2000 && d->year <= 2099 && d->month >= 1 && d->month <= 12 &&
           d->day >= 1 && d->day <= days[d->month - 1] + (d->month == 2 && leap(d->year)) &&
           d->hour < 24 && d->minute < 60 && d->second < 60 && d->weekday >= 1 && d->weekday <= 7;
}
// Calcule dimanche=1 ... samedi=7 pour une date dont année/mois/jour sont valides.
// La table t et le décalage de janvier/février prennent en compte les années bissextiles.
uint8_t Time_Weekday(const DateTime *d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = d->year;
    if (d->month < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + t[d->month - 1] + d->day) % 7 + 1;
}
#ifndef HOST_TEST
#include "grove_rtc_ds1307.h"
// Lit le matériel puis remplit *d ; seule une réponse vraie rend la date exploitable.
// Le pilote peut répondre à l'I2C tout en refusant une horloge arrêtée ou un calendrier invalide.
bool Time_Read(DateTime *d) {
    RTC_DateTime rtc;
    if (!d || !GroveRTC_GetDateTime(&rtc))
        return false;
    *d = (DateTime){rtc.year,    rtc.month,   rtc.day,      rtc.hours,
                    rtc.minutes, rtc.seconds, rtc.dayOfWeek};
    return Time_Valid(d);
}
// Écrit une date validée dans la RTC ; const indique que la structure source reste intacte.
// Renvoie le succès du transfert matériel, sans inventer de date de remplacement.
bool Time_Write(const DateTime *d) {
    if (!Time_Valid(d))
        return false;
    RTC_DateTime rtc = {.year = d->year,
                        .month = d->month,
                        .day = d->day,
                        .hours = d->hour,
                        .minutes = d->minute,
                        .seconds = d->second,
                        .dayOfWeek = d->weekday};
    return GroveRTC_SetDateTime(&rtc);
}
#endif
