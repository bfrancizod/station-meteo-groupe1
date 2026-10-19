/* Pilote DS1307 fourni, complété pour refuser date invalide et oscillateur arrêté. */
#include "grove_rtc_ds1307.h"
#include "board_pins.h"
extern I2C_HandleTypeDef GROVE_I2C_HANDLE;
static bool rtc_valid(const RTC_DateTime *d) {
    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return d && d->year >= 2000 && d->year <= 2099 && d->month >= 1 && d->month <= 12 &&
           d->day >= 1 && d->day <= days[d->month - 1] + (d->month == 2 && d->year % 4 == 0) &&
           d->hours < 24 && d->minutes < 60 && d->seconds < 60 && d->dayOfWeek >= 1 &&
           d->dayOfWeek <= 7;
}
bool GroveRTC_Init(void) {
    return HAL_I2C_IsDeviceReady(&GROVE_I2C_HANDLE, 0x68 << 1, 1, 15) == HAL_OK;
}
static uint8_t dec(uint8_t v) {
    return (v >> 4) * 10 + (v & 15);
}
static uint8_t bcd(uint8_t v) {
    return (v / 10) * 16 + v % 10;
}
static bool bcd_ok(uint8_t v) {
    return (v & 15) < 10 && (v >> 4) < 10;
}
bool GroveRTC_GetDateTime(RTC_DateTime *d) {
    if (!d)
        return false;
    uint8_t b[7];
    if (HAL_I2C_Mem_Read(&hi2c1, 0x68 << 1, 0, 1, b, 7, 15) != HAL_OK || (b[0] & 0x80))
        return false;
    if (!bcd_ok(b[0] & 0x7f) || !bcd_ok(b[1]) || !bcd_ok(b[2] & 0x3f) || !bcd_ok(b[4]) ||
        !bcd_ok(b[5]) || !bcd_ok(b[6]))
        return false;
    d->seconds = dec(b[0] & 0x7f);
    d->minutes = dec(b[1]);
    if (b[2] & 0x40) {
        unsigned h = dec(b[2] & 0x1f);
        if (h < 1 || h > 12)
            return false;
        d->hours = (h % 12) + ((b[2] & 0x20) ? 12 : 0);
    } else
        d->hours = dec(b[2] & 0x3f);
    d->dayOfWeek = b[3];
    d->day = dec(b[4]);
    d->month = dec(b[5]);
    d->year = 2000 + dec(b[6]);
    return rtc_valid(d);
}
bool GroveRTC_SetDateTime(const RTC_DateTime *d) {
    if (!rtc_valid(d))
        return false;
    uint8_t b[] = {bcd(d->seconds), bcd(d->minutes), bcd(d->hours),      d->dayOfWeek,
                   bcd(d->day),     bcd(d->month),   bcd(d->year - 2000)};
    return HAL_I2C_Mem_Write(&hi2c1, 0x68 << 1, 0, 1, b, 7, 20) == HAL_OK;
}
