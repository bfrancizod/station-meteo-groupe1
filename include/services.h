#ifndef SERVICES_H
#define SERVICES_H
#include "app_types.h"
/* Configuration : une copie temporaire est validée puis sauvegardée. */
void Config_Defaults(Config *c);
bool Config_Validate(const Config *c);
bool Config_Set(Config *c, const char *key, const char *value);
bool Nv_Load(Config *c);
bool Nv_Save(const Config *c);
uint32_t Crc32(const void *data, uint32_t size);
/* RTC et calendrier (weekday : dimanche=1 ... samedi=7). */
bool Time_Valid(const DateTime *d);
bool Time_Read(DateTime *d);
bool Time_Write(const DateTime *d);
uint8_t Time_Weekday(const DateTime *d);
/* BME280 : init puis déclenchement forcé ; lecture après conversion. */
bool Env_Init(void);
bool Env_Begin(void);
bool Env_Read(float *t, float *h, float *p);
void Gps_Process(void);
bool Gps_Take(uint32_t since, GpsFix *out);
bool Gps_Parse(const char *line, GpsFix *out);
void Gps_Reset(void);
/* SD : les fichiers sont fermés après chaque ligne. */
bool Storage_Mount(void);
void Storage_Release(void);
uint32_t Storage_Write(const Measurement *m, uint32_t max_size);
int Measurement_Format(const Measurement *m, char *out, unsigned size);
void App_Init(void);
void App_Process(uint32_t now);
void App_Background(void); /* Appels SD longs : boutons, GPS, console et LED. */
#endif
