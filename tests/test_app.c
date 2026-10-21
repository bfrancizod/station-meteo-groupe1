// Rôle : Vérifie les acquisitions, les changements de mode et les accès SD avec du matériel simulé.
/* Test d'intégration du superviseur avec périphériques simulés.
 * Il ne valide ni les niveaux électriques, ni les transactions des pilotes. */
#include "board.h"
#include "services.h"
#include "grove_rgb.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t clock_ms;
static bool red_pressed, green_pressed;
static unsigned writes, mounts, releases;
static char log_output[8192];
static const char *input = "";
// Ces états simulent les pannes séparément ; aucune broche réelle n'est commandée par ces tests.
static bool gps_available = true, env_available = true, rtc_valid = true;
static uint16_t stored_timeout_s;
static uint32_t storage_error;
// Conserver la dernière acquisition et la couleur demandée permet de vérifier les résultats.
static Measurement last_measurement;
static uint8_t led_r, led_g, led_b;
int hi2c1;
uint32_t HAL_GetTick(void) {
    return clock_ms;
}
unsigned HAL_GPIO_ReadPin(void *port, unsigned pin) {
    return 1;
}
int HAL_I2C_IsDeviceReady(void *bus, unsigned address, unsigned trials, unsigned timeout) {
    return 0;
}
void Board_ConsoleWrite(const char *s) {
    assert(strlen(log_output) + strlen(s) < sizeof(log_output));
    strcat(log_output, s);
}
int Board_ConsoleGet(void) {
    return *input ? *input++ : -1;
}
bool Board_Red(void) {
    return red_pressed;
}
bool Board_Green(void) {
    return green_pressed;
}
uint32_t Board_RxOverflows(void) {
    return 0;
}
uint16_t Board_Light(void) {
    return 2048;
}
bool Nv_Load(Config *c) {
    if (stored_timeout_s) {
        c->timeout_s = stored_timeout_s;
        return true;
    }
    return false;
}
bool Nv_Save(const Config *c) {
    return true;
}
bool Time_Read(DateTime *d) {
    *d = (DateTime){2026, 10, 16, 12, 0, 0, 6};
    return rtc_valid;
}
bool Time_Write(const DateTime *d) {
    return true;
}
bool Env_Init(void) {
    return env_available;
}
bool Env_Begin(void) {
    return env_available;
}
bool Env_Read(float *t, float *h, float *p) {
    *t = 20;
    *h = 50;
    *p = 1013;
    return env_available;
}
void Gps_Process(void) {
}
void Gps_Reset(void) {
}
bool Gps_Take(uint32_t since, GpsFix *out) {
    if (!gps_available)
        return false;
    *out = (GpsFix){48.7, 6.2, 1, 8};
    return true;
}
bool Storage_Mount(void) {
    mounts++;
    return true;
}
void Storage_Release(void) {
    releases++;
}
uint32_t Storage_Write(const Measurement *m, uint32_t size) {
    last_measurement = *m;
    writes++;
    return m->date_valid ? storage_error : ERR_RTC;
}
void GroveRGB_Init(void) {
}
void GroveRGB_SetColor(uint8_t r, uint8_t g, uint8_t b) {
    led_r = r;
    led_g = g;
    led_b = b;
}
void GroveRGB_Off(void) {
}
static void step(uint32_t t) {
    clock_ms = t;
    App_Process(t);
}
// Chaque commande vide les anciens diagnostics pour vérifier précisément sa réponse.
static void command(const char *line, uint32_t t) {
    log_output[0] = 0;
    input = line;
    step(t);
}
int main(int argc, char **argv) {
    if (argc > 1) {
        assert(argc == 2 && !strcmp(argv[1], "--short-timeout"));
        stored_timeout_s = 10;
    }
    App_Init();
    step(0);
    step(100);
    assert(writes == 1);
    assert(last_measurement.date_valid);
    assert(last_measurement.status[SENSOR_TEMP] == VALUE_OK);
    assert(last_measurement.light == 512);
    red_pressed = true;
    step(200);
    step(240);
    step(5241);
    assert(releases == 1);
    assert(strstr(log_output, "MODE MAINTENANCE"));
    assert(writes == 1);
    red_pressed = false;
    step(5300);
    step(5340);
    red_pressed = true;
    step(5400);
    step(5440);
    step(10441);
    assert(mounts == 2);
    assert(strstr(log_output, "SD remontee"));
    red_pressed = false;
    step(10500);
    step(10540);
    step(10600);
    assert(writes == 2);
    green_pressed = true;
    step(10700);
    step(10740);
    step(15741);
    assert(strstr(log_output, "MODE ECONOMIQUE"));
    green_pressed = false;
    step(15800);
    step(15840);
    /* Un seul appui rouge en économie revient au standard, suivant l'hypothèse A1. */
    red_pressed = true;
    step(15900);
    step(15940);
    step(20941);
    input = "STATUS\n";
    step(21000);
    assert(strstr(log_output, "MODE=STANDARD"));
    puts("Supervision simulee : acquisition, maintenance, liberation SD, retour et economie : OK");

    red_pressed = false;
    step(21100);
    step(21140);
    uint32_t t = 22000;
    uint32_t timeout_ms = (stored_timeout_s ? stored_timeout_s : 30u) * 1000u;
    gps_available = false;
    // Sans option, les deux échecs successifs déclenchent toujours la vraie erreur GPS.
    for (unsigned i = 0; i < 2; i++) {
        unsigned before = writes;
        command("MEASURE\n", t);
        step(t + 100);
        step(t + timeout_ms - 1);
        assert(writes == before);
        step(t + timeout_ms);
        assert(writes == before + 1);
        assert(last_measurement.status[SENSOR_GPS] == VALUE_TIMEOUT);
        t += timeout_ms + 1000;
    }
    command("STATUS\n", t);
    assert(strstr(log_output, "ERR=0x02") && strstr(log_output, "GPS_DEMO=0"));
    command("GPS_DEMO=2\n", t);
    assert(strstr(log_output, "ERREUR"));

    // Le remplacement n'arrive pas avant 15 s, même avec un TIMEOUT de 10 ou 30 s.
    unsigned before = writes;
    command("GPS_DEMO=1\n", t);
    step(t + 100);
    command("GPS_DEMO=1\n", t + 1000);
    step(t + 14999);
    assert(writes == before);
    step(t + 15000);
    assert(writes == before + 1);
    assert(last_measurement.status[SENSOR_GPS] == VALUE_SIMULATED);
    assert(last_measurement.gps.lat == 0 && last_measurement.gps.lon == 0);
    assert(last_measurement.gps.quality == 0 && last_measurement.gps.satellites == 0);
    assert(last_measurement.status[SENSOR_TEMP] == VALUE_OK);
    assert(strstr(log_output, "position fictive 0,0 (DEMO)"));
    assert(led_r == 0 && led_g == 50 && led_b == 0);
    step(t + 15500);
    assert(led_r == 0 && led_g == 50 && led_b == 0);
    command("STATUS\n", t + 15500);
    assert(strstr(log_output, "ERR=0x00") && strstr(log_output, "GPS_DEMO=1"));
    t += 16000;

    // Une position réelle à l'échéance reste prioritaire ; le cycle suivant réessaie le GPS.
    command("MEASURE\n", t);
    step(t + 100);
    gps_available = true;
    step(t + 15000);
    assert(last_measurement.status[SENSOR_GPS] == VALUE_OK);
    assert(last_measurement.gps.lat == 48.7 && last_measurement.gps.lon == 6.2);
    t += 16000;
    command("MEASURE\n", t);
    before = writes;
    step(t + 100);
    assert(writes == before + 1 && last_measurement.status[SENSOR_GPS] == VALUE_OK);
    t += 1000;

    // L'option ne masque pas les erreurs SD, RTC ou des autres capteurs.
    gps_available = false;
    storage_error = ERR_SD_IO;
    command("MEASURE\n", t);
    step(t + 100);
    step(t + 15000);
    command("STATUS\n", t + 15000);
    assert(strstr(log_output, "ERR=0x20"));
    t += 16000;
    storage_error = 0;
    rtc_valid = false;
    command("MEASURE\n", t);
    step(t + 100);
    step(t + 15000);
    command("STATUS\n", t + 15000);
    assert(strstr(log_output, "ERR=0x01"));
    t += 16000;
    rtc_valid = true;
    env_available = false;
    uint32_t wait_ms = timeout_ms > 15000 ? timeout_ms : 15000;
    for (unsigned i = 0; i < 2; i++) {
        before = writes;
        command("MEASURE\n", t);
        step(t + 100);
        step(t + 14999);
        step(t + 15000);
        if (wait_ms > 15000)
            assert(writes == before);
        step(t + wait_ms);
        assert(writes == before + 1);
        assert(last_measurement.status[SENSOR_GPS] == VALUE_SIMULATED);
        assert(last_measurement.status[SENSOR_TEMP] == VALUE_TIMEOUT);
        t += wait_ms + 1000;
    }
    command("STATUS\n", t);
    assert(strstr(log_output, "ERR=0x04"));

    // La désactivation restaure le délai normal et les défauts GPS, sans données fictives.
    env_available = true;
    for (unsigned i = 0; i < 2; i++) {
        command(i ? "MEASURE\n" : "GPS_DEMO=0\n", t);
        step(t + 100);
        step(t + timeout_ms);
        assert(last_measurement.status[SENSOR_GPS] == VALUE_TIMEOUT);
        t += timeout_ms + 1000;
    }
    command("STATUS\n", t);
    assert(strstr(log_output, "ERR=0x02") && strstr(log_output, "GPS_DEMO=0"));

    // Le calcul des 15 s résiste au rebouclage du compteur milliseconde sur 32 bits.
    t = UINT32_MAX - 7000u;
    before = writes;
    command("GPS_DEMO=1\n", t);
    step(t + 100u);
    step(t + 14999u);
    assert(writes == before);
    step(t + 15000u);
    assert(writes == before + 1 && last_measurement.status[SENSOR_GPS] == VALUE_SIMULATED);
    printf("GPS demonstration : delai 15 s, vraie position prioritaire, defauts et retour normal : OK (TIMEOUT=%lu s)\n",
           (unsigned long)(timeout_ms / 1000u));
    return 0;
}
