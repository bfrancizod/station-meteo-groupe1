#ifndef APP_TYPES_H
#define APP_TYPES_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { MODE_STANDARD, MODE_CONFIGURATION, MODE_MAINTENANCE, MODE_ECONOMIC } AppMode;
typedef enum {
    VALUE_OK,
    VALUE_DISABLED,
    VALUE_SKIPPED,
    VALUE_TIMEOUT,
    VALUE_IO_ERROR,
    VALUE_INVALID
} ValueStatus;
typedef enum {
    SENSOR_LIGHT,
    SENSOR_TEMP,
    SENSOR_HUM,
    SENSOR_PRESS,
    SENSOR_GPS,
    SENSOR_COUNT
} SensorId;
typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second, weekday;
} DateTime;
typedef struct {
    double lat, lon;
    uint8_t quality, satellites;
} GpsFix;
typedef struct {
    uint16_t interval_min, timeout_s;
    uint32_t file_max;
    uint8_t lumin, temp, hygr, pressure;
    uint16_t lumin_low, lumin_high;
    float temp_min, temp_max, hygr_mint, hygr_maxt, pressure_min, pressure_max;
} Config;
typedef struct {
    DateTime date;
    bool date_valid;
    AppMode mode;
    float temperature, humidity, pressure;
    uint16_t light;
    GpsFix gps;
    ValueStatus status[SENSOR_COUNT];
} Measurement;
/* Un bit par défaut. Les capteurs conservent leurs défauts individuellement. */
enum {
    ERR_RTC = 1,
    ERR_GPS = 2,
    ERR_SENSOR_ACCESS = 4,
    ERR_SENSOR_INVALID = 8,
    ERR_SD_FULL = 16,
    ERR_SD_IO = 32
};
#endif
