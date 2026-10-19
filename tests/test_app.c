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
    return false;
}
bool Nv_Save(const Config *c) {
    return true;
}
bool Time_Read(DateTime *d) {
    *d = (DateTime){2026, 10, 16, 12, 0, 0, 6};
    return true;
}
bool Time_Write(const DateTime *d) {
    return true;
}
bool Env_Init(void) {
    return true;
}
bool Env_Begin(void) {
    return true;
}
bool Env_Read(float *t, float *h, float *p) {
    *t = 20;
    *h = 50;
    *p = 1013;
    return true;
}
void Gps_Process(void) {
}
void Gps_Reset(void) {
}
bool Gps_Take(uint32_t since, GpsFix *out) {
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
    assert(m->date_valid);
    assert(m->status[SENSOR_TEMP] == VALUE_OK);
    assert(m->light == 512);
    writes++;
    return 0;
}
void GroveRGB_Init(void) {
}
void GroveRGB_SetColor(uint8_t r, uint8_t g, uint8_t b) {
}
void GroveRGB_Off(void) {
}
static void step(uint32_t t) {
    clock_ms = t;
    App_Process(t);
}
int main(void) {
    App_Init();
    step(0);
    step(100);
    assert(writes == 1);
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
    return 0;
}
