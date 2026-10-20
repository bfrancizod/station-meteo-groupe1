// Rôle : Déclare les services de configuration, d'acquisition, de stockage et de supervision.
// Contrats entre app.c et les modules de src : configuration, temps, capteurs et stockage.
// Les pointeurs de sortie désignent des objets fournis par l'appelant ; pas d'allocation ici.
#ifndef SERVICES_H
#define SERVICES_H
#include "app_types.h"
// Defaults remplit c ; Validate ne le modifie pas ; Set ne remplace c qu'après validation.
// Load laisse c intact si aucune copie Flash valide ; Save renvoie le succès de la sauvegarde.
/* Configuration : une copie temporaire est validée puis sauvegardée. */
void Config_Defaults(Config *c);
bool Config_Validate(const Config *c);
bool Config_Set(Config *c, const char *key, const char *value);
bool Nv_Load(Config *c);
bool Nv_Save(const Config *c);
// Empreinte CRC de size octets ; data est lu comme une suite d'octets.
uint32_t Crc32(const void *data, uint32_t size);
/* RTC et calendrier (weekday : dimanche=1 ... samedi=7). */
// Les retours booléens qualifient la date ; Weekday attend année/mois/jour déjà valides.
bool Time_Valid(const DateTime *d);
bool Time_Read(DateTime *d);
bool Time_Write(const DateTime *d);
uint8_t Time_Weekday(const DateTime *d);
// Init détecte/configure ; Begin déclenche ; Read remplit t/h/p en °C, % et hPa si vrai.
/* BME280 : init puis déclenchement forcé ; lecture après conversion. */
bool Env_Init(void);
bool Env_Begin(void);
bool Env_Read(float *t, float *h, float *p);
// Process ne prend que les octets déjà disponibles ; Reset invalide et vide l'ancien flux.
// Take exige une position valide reçue depuis since (ms HAL) et datant d'au plus 2 s.
// Parse valide une phrase GGA sous forme de texte.
void Gps_Process(void);
bool Gps_Take(uint32_t since, GpsFix *out);
bool Gps_Parse(const char *line, GpsFix *out);
void Gps_Reset(void);
// Mount vérifie le volume FAT ; Release le désenregistre après fermeture des fichiers.
// Write renvoie 0 ou ERR_*, y compris ERR_RTC ; max_size est en octets.
/* SD : les fichiers sont fermés après chaque ligne. */
bool Storage_Mount(void);
void Storage_Release(void);
uint32_t Storage_Write(const Measurement *m, uint32_t max_size);
// Écrit le format compact dans out (size inclut '\0') ; longueur utile, ou -1 si insuffisant.
int Measurement_Format(const Measurement *m, char *out, unsigned size);
// Init au démarrage ; Process à chaque boucle avec now en ms ; Background pendant les attentes.
// Background n'exécute pas les commandes console malgré le commentaire historique ci-dessous.
void App_Init(void);
void App_Process(uint32_t now);
void App_Background(void); /* Appels SD longs : boutons, GPS, console et LED. */
#endif
