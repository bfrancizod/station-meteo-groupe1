/* DS1307 : calendrier strict ; aucune date fictive injectée au démarrage. */
#include "services.h"
#ifndef HOST_TEST
#include "board.h"
#endif
static bool leap(unsigned y) {
    return y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
}
bool Time_Valid(const DateTime *d) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return d && d->year >= 2000 && d->year <= 2099 && d->month >= 1 && d->month <= 12 &&
           d->day >= 1 && d->day <= days[d->month - 1] + (d->month == 2 && leap(d->year)) &&
           d->hour < 24 && d->minute < 60 && d->second < 60 && d->weekday >= 1 && d->weekday <= 7;
}
uint8_t Time_Weekday(const DateTime *d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int y = d->year;
    if (d->month < 3)
        y--;
    return (y + y / 4 - y / 100 + y / 400 + t[d->month - 1] + d->day) % 7 + 1;
}
#ifndef HOST_TEST
#include "grove_rtc_ds1307.h"
bool Time_Read(DateTime *d) {
    RTC_DateTime rtc;
    if (!d || !GroveRTC_GetDateTime(&rtc))
        return false;
    *d = (DateTime){rtc.year,    rtc.month,   rtc.day,      rtc.hours,
                    rtc.minutes, rtc.seconds, rtc.dayOfWeek};
    return Time_Valid(d);
}
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
