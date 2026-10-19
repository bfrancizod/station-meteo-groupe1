/* Adaptation applicative des interfaces Grove : les types fournis sont réutilisés. */
#include "services.h"
#include "grove_bme280.h"
bool Env_Init(void) {
    return GroveBME280_Init();
}
bool Env_Begin(void) {
    return GroveBME280_Begin();
}
bool Env_Read(float *t, float *h, float *p) {
    Env_Data data;
    if (!GroveBME280_ReadEnvironment(&data))
        return false;
    *t = data.temperature_c;
    *h = data.humidity_percent;
    *p = data.pressure_hpa;
    return true;
}
