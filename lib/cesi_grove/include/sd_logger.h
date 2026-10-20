// Rôle : Déclare les fonctions d'initialisation, de diagnostic et d'écriture de la carte SD.
// Interface du pilote SD de la base, utilisée par diskio_spi.c et storage.c.
// Init concerne le protocole SPI ; WriteLine nécessite déjà un volume FAT monté.
#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <stdbool.h>

// SDSC utilise des adresses d'octets, SDHC des adresses de secteurs ; UNKNOWN = non identifié.
typedef enum
{
	SDLOGGER_CARD_UNKNOWN = 0,
	SDLOGGER_CARD_SDSC,
	SDLOGGER_CARD_SDHC
} SDLoggerCardType;

// Étape du dernier résultat d'initialisation, distincte des erreurs de fichier FatFs.
typedef enum
{
	SDLOGGER_INIT_OK = 0,
	SDLOGGER_INIT_ERR_CMD0,
	SDLOGGER_INIT_ERR_CMD8_PATTERN,
	SDLOGGER_INIT_ERR_ACMD41_TIMEOUT,
	SDLOGGER_INIT_ERR_CMD58,
	SDLOGGER_INIT_ERR_CMD1_TIMEOUT
} SDLoggerInitStatus;

// Init est synchrone ; les accesseurs suivants renvoient ses informations mémorisées.
bool SDLogger_Init(void);
SDLoggerCardType SDLogger_GetCardType(void);
SDLoggerInitStatus SDLogger_GetLastInitStatus(void);
const char *SDLogger_GetActiveCsLabel(void);
// Diagnostic matériel optionnel, jamais appelé par le cycle normal ; succès limité à CMD0.
bool SDLogger_BitBangCMD0Test(void);
// Ajoute line telle quelle, ferme/synchronise ; vrai seulement si tous les octets sont écrits.
bool SDLogger_WriteLine(const char *filename, const char *line);

#endif
