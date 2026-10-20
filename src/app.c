/* Supervision coopérative : aucun délai de plusieurs minutes.
 * Les services matériels sont courts ; la SD appelle App_Background pendant
 * ses attentes pour garder boutons, GPS et signalisation disponibles. */
#include "board.h"
#include "grove_rgb.h"
#include "services.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define VERSION "WWW-G1-1.1.0"
#define LOT "CESI-G1-2026"
static Config cfg;
static AppMode mode, previous_mode;
static uint32_t next_cycle, last_activity, cycle_start, env_due, light_due;
static uint32_t errors, sd_error;
static bool initialized, cycle, env_ok, env_pending, request_gps, force_measure;
static unsigned economic_cycle;
static Measurement measurement;
static uint8_t timeout_count[SENSOR_COUNT];
static uint8_t sensor_access, sensor_invalid;
static char command_line[96];
static unsigned command_used;
static bool command_overflow;
static bool last_was_cr;
typedef struct {
    bool raw, stable, sent;
    uint32_t changed, pressed;
} Button;
static Button red, green;
static uint8_t events;
static void output(const char *s) {
    Board_ConsoleWrite(s);
}
static const char *mode_name(void) {
    static const char *names[] = {"STANDARD", "CONFIGURATION", "MAINTENANCE", "ECONOMIQUE"};
    return names[mode];
}
static uint32_t period(void) {
    return (uint32_t)cfg.interval_min * 60000u * (mode == MODE_ECONOMIC ? 2u : 1u);
}
static void button_process(Button *b, bool raw, uint32_t now, uint8_t event) {
    if (raw != b->raw) {
        b->raw = raw;
        b->changed = now;
    }
    if (raw != b->stable && now - b->changed >= 30) {
        b->stable = raw;
        if (raw)
            b->pressed = now;
        else
            b->sent = false;
    }
    if (b->stable && !b->sent && now - b->pressed >= 5000) {
        events |= event;
        b->sent = true;
    }
}
static void led(uint32_t now) {
    uint8_t r = 0, g = 0, b = 0;
    uint32_t phase = now % 1000;
    if (errors) {
        unsigned code = (errors & ERR_RTC)              ? 0
                        : (errors & ERR_SD_IO)          ? 1
                        : (errors & ERR_SD_FULL)        ? 2
                        : (errors & ERR_SENSOR_ACCESS)  ? 3
                        : (errors & ERR_SENSOR_INVALID) ? 4
                                                        : 5;
        unsigned red_time = (code == 1 || code == 4) ? 333 : 500;
        if (phase < red_time)
            r = 50;
        else
            switch (code) {
            case 0:
                b = 50;
                break;
            case 1:
            case 2:
                r = g = b = 50;
                break;
            case 3:
            case 4:
                g = 50;
                break;
            default:
                r = g = 50;
                break;
            }
    } else
        switch (mode) {
        case MODE_STANDARD:
            g = 50;
            break;
        case MODE_CONFIGURATION:
            r = g = 50;
            break;
        case MODE_ECONOMIC:
            b = 50;
            break;
        case MODE_MAINTENANCE:
            r = 50;
            g = 15;
            break;
        }
    static uint32_t old = 0xffffffff;
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
    if (color != old) {
        GroveRGB_SetColor(r, g, b);
        old = color;
    }
}
void App_Background(void) {
    if (!initialized)
        return;
    uint32_t now = HAL_GetTick();
    Gps_Process();
    button_process(&red, Board_Red(), now, 1);
    button_process(&green, Board_Green(), now, 2);
    led(now);
    /* Les commandes sont exécutées dans App_Process, jamais dans un appel FatFs. */
}
/* Une transition annule la mesure non publiée et libère la SD dans la boucle, jamais en
 * interruption. */
static void set_mode(AppMode target, uint32_t now) {
    if (mode == target)
        return;
    cycle = false;
    force_measure = false;
    if (target == MODE_MAINTENANCE) {
        previous_mode = mode;
        Storage_Release();
        output("SD liberee : retrait autorise.\r\n");
        sd_error = 0;
    }
    if (mode == MODE_MAINTENANCE) {
        sd_error = Storage_Mount() ? 0 : ERR_SD_IO;
        output(sd_error ? "ERREUR remontage SD\r\n" : "SD remontee\r\n");
    }
    mode = target;
    economic_cycle = 0;
    next_cycle = now;
    last_activity = now;
    char s[64];
    snprintf(s, sizeof(s), "MODE %s\r\n", mode_name());
    output(s);
}
static void show_config(void) {
    char s[512];
    snprintf(
        s, sizeof(s),
        "LOG_INTERVAL=%u\r\nFILE_MAX_SIZE=%lu\r\nTIMEOUT=%u\r\nLUMIN=%u\r\nLUMIN_LOW=%u\r\nLUMIN_"
        "HIGH=%u\r\nTEMP_AIR=%u\r\nMIN_TEMP_AIR=%.2f\r\nMAX_TEMP_AIR=%.2f\r\nHYGR=%u\r\nHYGR_MINT=%"
        ".2f\r\nHYGR_MAXT=%.2f\r\nPRESSURE=%u\r\nPRESSURE_MIN=%.2f\r\nPRESSURE_MAX=%.2f\r\n",
        cfg.interval_min, (unsigned long)cfg.file_max, cfg.timeout_s, cfg.lumin, cfg.lumin_low,
        cfg.lumin_high, cfg.temp, (double)cfg.temp_min, (double)cfg.temp_max, cfg.hygr,
        (double)cfg.hygr_mint, (double)cfg.hygr_maxt, cfg.pressure, (double)cfg.pressure_min,
        (double)cfg.pressure_max);
    output(s);
}
/* Modifier une composante après validation ; une RTC absente ne crée pas de date automatique. */
static bool rtc_command(const char *key, const char *value) {
    DateTime d;
    bool valid = Time_Read(&d);
    static DateTime draft = {2000, 1, 1, 0, 0, 0, 7};
    if (!valid)
        d = draft;
    unsigned a, b, c;
    char extra;
    if (!strcmp(key, "CLOCK")) {
        if (sscanf(value, "%u:%u:%u%c", &a, &b, &c, &extra) != 3 || a > 23 || b > 59 || c > 59)
            return false;
        d.hour = a;
        d.minute = b;
        d.second = c;
    } else if (!strcmp(key, "DATE")) {
        if (sscanf(value, "%u,%u,%u%c", &a, &b, &c, &extra) != 3 || a < 1 || a > 12 || b < 1 ||
            b > 31 || c < 2000 || c > 2099)
            return false;
        d.month = a;
        d.day = b;
        d.year = c;
        if (!Time_Valid(&d))
            return false;
        d.weekday = Time_Weekday(&d);
    } else if (!strcmp(key, "DAY")) {
        static const char *names[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
        unsigned i;
        for (i = 0; i < 7 && strcmp(names[i], value); i++) {
        }
        if (i == 7)
            return false;
        d.weekday = i + 1;
    } else
        return false;
    if (!Time_Valid(&d))
        return false;
    /* Si RTC invalide, CLOCK seul ne publie pas une fausse date par défaut.
     * DATE doit explicitement définir la date avant la première écriture. */
    if (!valid && strcmp(key, "DATE")) {
        draft = d;
        output("RTC non reglee : saisir DATE=MM,JJ,AAAA puis CLOCK=HH:MM:SS\r\n");
        return false;
    }
    draft = d;
    if (!Time_Write(&d))
        return false;
    errors &= ~ERR_RTC;
    return true;
}
/* Analyser une ligne complète ; appliquer les réglages après leur sauvegarde effective. */
static void execute(char *line) {
    if (!*line)
        return;
    if (!strcmp(line, "HELP")) {
        output("HELP STATUS SHOW VERSION I2C MEASURE\r\nConfiguration : PARAM=valeur RESET "
               "CLOCK=HH:MM:SS DATE=MM,JJ,AAAA DAY=MON\r\n");
        return;
    }
    if (!strcmp(line, "VERSION")) {
        output(VERSION " LOT=" LOT "\r\n");
        return;
    }
    if (!strcmp(line, "SHOW")) {
        show_config();
        return;
    }
    if (!strcmp(line, "STATUS")) {
        char s[256];
        DateTime d;
        bool rtc = Time_Read(&d);
        if (rtc)
            errors &= ~ERR_RTC;
        else
            errors |= ERR_RTC;
        snprintf(
            s, sizeof(s),
            "MODE=%s ERR=0x%02lX RAW_PA10=%u RAW_PB3=%u RED=%u GREEN=%u RX_TX_LOST=%lu RTC=%s\r\n",
            mode_name(), (unsigned long)errors, HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_10),
            HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_3), Board_Red(), Board_Green(),
            (unsigned long)Board_RxOverflows(), rtc ? "OK" : "INVALIDE/ABSENTE");
        output(s);
        if (rtc) {
            snprintf(s, sizeof(s), "RTC=%04u-%02u-%02u %02u:%02u:%02u\r\n", d.year, d.month, d.day,
                     d.hour, d.minute, d.second);
            output(s);
        }
        return;
    }
    if (!strcmp(line, "I2C")) {
        output("I2C presents :");
        for (unsigned a = 1; a < 127; a++) {
            if (HAL_I2C_IsDeviceReady(&hi2c1, a << 1, 1, 3) == HAL_OK) {
                char s[8];
                snprintf(s, sizeof(s), " %02X", a);
                output(s);
            }
            App_Background();
        }
        output("\r\nAttendus : 68 et 76/77\r\n");
        return;
    }
    if (!strcmp(line, "MEASURE")) {
        if (mode == MODE_CONFIGURATION || cycle)
            output("REFUS : configuration ou acquisition en cours\r\n");
        else {
            force_measure = true;
            output("Mesure de diagnostic demandee\r\n");
        }
        return;
    }
    if (mode != MODE_CONFIGURATION) {
        output("ERREUR : reglage reserve au mode CONFIGURATION\r\n");
        return;
    }
    if (!strcmp(line, "RESET")) {
        Config n;
        Config_Defaults(&n);
        if (Nv_Save(&n)) {
            cfg = n;
            output("OK RESET\r\n");
        } else
            output("ERREUR FLASH, configuration conservee\r\n");
        return;
    }
    char *eq = strchr(line, '=');
    if (!eq) {
        output("ERREUR syntaxe : HELP\r\n");
        return;
    }
    *eq++ = 0;
    if (!strcmp(line, "CLOCK") || !strcmp(line, "DATE") || !strcmp(line, "DAY")) {
        output(rtc_command(line, eq) ? "OK RTC\r\n" : "ERREUR RTC ou format\r\n");
        return;
    }
    Config n = cfg;
    if (!Config_Set(&n, line, eq)) {
        output("ERREUR parametre, valeur ou seuils\r\n");
        return;
    }
    if (!Nv_Save(&n)) {
        output("ERREUR FLASH : ancien reglage conserve\r\n");
        return;
    }
    cfg = n;
    output("OK parametre sauvegarde\r\n");
}
static void console_process(uint32_t now) {
    int c;
    while ((c = Board_ConsoleGet()) >= 0) {
        last_activity = now;
        if (c == '\n' && last_was_cr) {
            last_was_cr = false;
            continue;
        }
        last_was_cr = c == '\r';
        if (c == '\r' || c == '\n') {
            if (command_overflow)
                output("ERREUR ligne trop longue\r\n");
            else {
                command_line[command_used] = 0;
                execute(command_line);
            }
            command_used = 0;
            command_overflow = false;
        } else if (c == 8 || c == 127) {
            if (command_used)
                command_used--;
        } else if (c >= 32 && c <= 126 && !command_overflow) {
            if (command_used < sizeof(command_line) - 1)
                command_line[command_used++] = c;
            else
                command_overflow = true;
        }
    }
}
/* Réinitialiser les statuts pour ne jamais republier une valeur ancienne comme mesure récente. */
static void begin_cycle(uint32_t now) {
    memset(&measurement, 0, sizeof(measurement));
    measurement.mode = mode;
    for (unsigned i = 0; i < SENSOR_COUNT; i++)
        measurement.status[i] = VALUE_DISABLED;
    if (cfg.lumin)
        measurement.status[SENSOR_LIGHT] = VALUE_TIMEOUT;
    if (cfg.temp)
        measurement.status[SENSOR_TEMP] = VALUE_TIMEOUT;
    if (cfg.hygr)
        measurement.status[SENSOR_HUM] = VALUE_TIMEOUT;
    if (cfg.pressure)
        measurement.status[SENSOR_PRESS] = VALUE_TIMEOUT;
    request_gps = mode != MODE_ECONOMIC || economic_cycle % 2 == 0;
    economic_cycle++;
    measurement.status[SENSOR_GPS] = request_gps ? VALUE_TIMEOUT : VALUE_SKIPPED;
    cycle_start = now;
    light_due = env_due = now;
    env_pending = false;
    cycle = true;
    Gps_Reset();
    next_cycle = now + period();
}
/* Mémoriser les défauts par source, lire la RTC en fin de collecte et publier la mesure. */
static void finish_cycle(uint32_t now) {
    for (unsigned i = 0; i < SENSOR_COUNT; i++) {
        ValueStatus s = measurement.status[i];
        uint8_t bit = 1u << i;
        if (s == VALUE_DISABLED) {
            sensor_access &= ~bit;
            sensor_invalid &= ~bit;
            timeout_count[i] = 0;
            continue;
        }
        if (s == VALUE_SKIPPED)
            continue;
        if (s == VALUE_TIMEOUT) {
            if (timeout_count[i] < 2)
                timeout_count[i]++;
            if (timeout_count[i] >= 2)
                sensor_access |= bit;
            sensor_invalid &= ~bit;
        } else if (s == VALUE_INVALID) {
            sensor_invalid |= bit;
            sensor_access &= ~bit;
            timeout_count[i] = 0;
        } else {
            sensor_access &= ~bit;
            sensor_invalid &= ~bit;
            timeout_count[i] = 0;
        }
    }
    measurement.date_valid = Time_Read(&measurement.date);
    errors = measurement.date_valid ? 0 : ERR_RTC;
    if (sensor_access & (1u << SENSOR_GPS))
        errors |= ERR_GPS;
    if (sensor_access & ~(1u << SENSOR_GPS))
        errors |= ERR_SENSOR_ACCESS;
    if (sensor_invalid)
        errors |= ERR_SENSOR_INVALID;
    if (mode == MODE_MAINTENANCE) {
        char line[256];
        /* Affichage lisible reserve a la console de maintenance. */
        if (Measurement_Format(&measurement, line, sizeof(line)) >= 0) {
            char *fields[8];
            unsigned count = 1;
            fields[0] = line;
            line[strcspn(line, "\r\n")] = '\0';
            for (char *p = line; *p; p++) {
                if (*p == ';') {
                    *p = '\0';
                    if (count < 8)
                        fields[count++] = p + 1;
                }
            }
            if (count == 8) {
                printf("\r\n"
                       "Date / heure : %s\r\n"
                       "Mode         : %s\r\n"
                       "Temperature  : %s deg C\r\n"
                       "Humidite     : %s %%\r\n"
                       "Pression     : %s hPa\r\n"
                       "Luminosite   : %s / 1023\r\n"
                       "Latitude     : %s\r\n"
                       "Longitude    : %s\r\n\r\n",
                       fields[0], fields[1], fields[2], fields[3],
                       fields[4], fields[5], fields[6], fields[7]);
            }
        }
        next_cycle = now + 1000;
    } else {
        sd_error = Storage_Write(&measurement, cfg.file_max);
        char s[96];
        snprintf(s, sizeof(s), "CYCLE %s SD=%s\r\n", mode_name(),
                 sd_error ? "ERREUR (STATUS)" : "OK");
        output(s);
    }
    errors |= sd_error;
    cycle = false;
}
/* Avancer les lectures sans attendre : lumière réessayée, conversion BME280 et GPS en continu. */
static void acquisition(uint32_t now) {
    if (cfg.lumin && measurement.status[SENSOR_LIGHT] == VALUE_TIMEOUT &&
        (int32_t)(now - light_due) >= 0) {
        uint16_t raw = Board_Light();
        light_due = now + 1000;
        if (raw != 0xffff) {
            measurement.light = ((uint32_t)raw * 1023 + 2047) / 4095;
            measurement.status[SENSOR_LIGHT] = VALUE_OK;
        }
    }
    bool env_needed = (cfg.temp && measurement.status[SENSOR_TEMP] == VALUE_TIMEOUT) ||
                      (cfg.hygr && measurement.status[SENSOR_HUM] == VALUE_TIMEOUT) ||
                      (cfg.pressure && measurement.status[SENSOR_PRESS] == VALUE_TIMEOUT);
    if (env_needed && (int32_t)(now - env_due) >= 0) {
        if (!env_ok)
            env_ok = Env_Init();
        if (!env_pending) {
            env_pending = env_ok && Env_Begin();
            env_due = now + (env_pending ? 100 : 1000);
        } else {
            float t, h, p;
            bool ok = Env_Read(&t, &h, &p);
            env_pending = false;
            env_due = now + 1000;
            if (!ok)
                env_ok = false;
            else {
                measurement.temperature = t;
                measurement.humidity = h;
                measurement.pressure = p;
                bool valid_t = t >= cfg.temp_min && t <= cfg.temp_max;
                if (cfg.temp)
                    measurement.status[SENSOR_TEMP] = valid_t ? VALUE_OK : VALUE_INVALID;
                if (cfg.hygr)
                    measurement.status[SENSOR_HUM] =
                        valid_t && t >= cfg.hygr_mint && t <= cfg.hygr_maxt && h >= 0 && h <= 100
                            ? VALUE_OK
                            : VALUE_INVALID;
                if (cfg.pressure)
                    measurement.status[SENSOR_PRESS] =
                        p >= cfg.pressure_min && p <= cfg.pressure_max ? VALUE_OK : VALUE_INVALID;
            }
        }
    }
    if (request_gps && measurement.status[SENSOR_GPS] == VALUE_TIMEOUT &&
        Gps_Take(cycle_start, &measurement.gps))
        measurement.status[SENSOR_GPS] = VALUE_OK;
    bool waiting = false;
    for (unsigned i = 0; i < SENSOR_COUNT; i++)
        if (measurement.status[i] == VALUE_TIMEOUT)
            waiting = true;
    if (!waiting || now - cycle_start >= (uint32_t)cfg.timeout_s * 1000)
        finish_cycle(now);
}
void App_Init(void) {
    GroveRGB_Init();
    Config_Defaults(&cfg);
    bool stored = Nv_Load(&cfg);
    uint32_t now = HAL_GetTick();
    red = (Button){.raw = Board_Red(),
                   .stable = Board_Red(),
                   .sent = Board_Red(),
                   .changed = now,
                   .pressed = now};
    green = (Button){.raw = Board_Green(),
                     .stable = Board_Green(),
                     .sent = Board_Green(),
                     .changed = now,
                     .pressed = now};
    mode = red.stable ? MODE_CONFIGURATION : MODE_STANDARD;
    previous_mode = MODE_STANDARD;
    next_cycle = now;
    last_activity = now;
    output("\r\n" VERSION " LOT=" LOT "\r\n");
    output(stored ? "Configuration Flash chargee\r\n"
                  : "Defauts charges (aucune configuration Flash valide)\r\n");
    initialized = true;
    env_ok = Env_Init();
    DateTime d;
    if (!Time_Read(&d))
        errors |= ERR_RTC;
    if (mode != MODE_CONFIGURATION)
        sd_error = Storage_Mount() ? 0 : ERR_SD_IO;
    errors |= sd_error;
    char s[80];
    snprintf(s, sizeof(s), "MODE %s ; HELP pour les commandes\r\n", mode_name());
    output(s);
}
void App_Process(uint32_t now) {
    App_Background();
    console_process(now);
    uint8_t event = events;
    events = 0;
    if (event & 1) {
        if (mode == MODE_STANDARD || mode == MODE_ECONOMIC)
            set_mode(mode == MODE_STANDARD ? MODE_MAINTENANCE : MODE_STANDARD, now);
        else if (mode == MODE_MAINTENANCE)
            set_mode(previous_mode, now);
    } else if ((event & 2) && mode == MODE_STANDARD)
        set_mode(MODE_ECONOMIC, now);
    if (mode == MODE_CONFIGURATION) {
        if (now - last_activity >= 1800000)
            set_mode(MODE_STANDARD, now);
        return;
    }
    if (!cycle && (force_measure || (int32_t)(now - next_cycle) >= 0)) {
        force_measure = false;
        begin_cycle(now);
    }
    if (cycle)
        acquisition(HAL_GetTick());
    led(HAL_GetTick());
}
