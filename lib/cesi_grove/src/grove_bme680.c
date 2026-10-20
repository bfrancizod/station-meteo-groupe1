// Pilote hérité conservé pour la base pédagogique ; le cycle actuel utilise le BME280.
// Seule l'identification I2C est disponible ici ; aucune mesure BME680 n'est produite.
#include "grove_bme680.h"
#include "board_pins.h"

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

#define BME680_ADDR_DEFAULT (0x76 << 1)
#define BME680_REG_ID      0xD0
#define BME680_REG_RESET   0xE0

static uint16_t bme680_addr = BME680_ADDR_DEFAULT;

// Sélectionne reg puis reçoit len octets dans data ; vrai si les deux échanges réussissent.
// bme680_addr est déjà décalée pour le format d'adresse attendu par la HAL.
static bool read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    if (HAL_I2C_Master_Transmit(&GROVE_I2C_HANDLE, bme680_addr, &reg, 1, 100) != HAL_OK)
        return false;

    return HAL_I2C_Master_Receive(&GROVE_I2C_HANDLE, bme680_addr, data, len, 100) == HAL_OK;
}

/* Écriture de registres non utilisée : le BME680 reste un pilote hérité. */

// Lit l'identifiant dans l'octet pointé par chip_id ; faux pour un pointeur nul ou un échec I2C.
bool GroveBME680_ReadChipId(uint8_t *chip_id)
{
    if (chip_id == 0) return false;
    return read_reg(BME680_REG_ID, chip_id, 1);
}

// Essaie les deux adresses et vérifie l'identifiant BME680 ; ne configure pas de compensation.
bool GroveBME680_Init(void)
{
    /*
     * BME680 standard chip ID = 0x61.
     * Adresse Seeed : 0x76 par défaut, parfois 0x77.
     */
    uint8_t id = 0;

    bme680_addr = (0x76 << 1);
    if (GroveBME680_ReadChipId(&id) && id == 0x61)
        return true;

    bme680_addr = (0x77 << 1);
    if (GroveBME680_ReadChipId(&id) && id == 0x61)
        return true;

    return false;
}

// Renvoie toujours faux sans remplir data : une interface présente ne garantit pas sa disponibilité.
bool GroveBME680_ReadEnvironment(Env_Data *data)
{
    (void)data;
    /* Pilote hérité non utilisé sur cette maquette : compensation BME680 non
     * implémentée dans le dépôt initial. Ne jamais renvoyer de fausses mesures.
     * Le vrai BME280 est géré séparément par GroveBME280_ReadEnvironment. */
    return false;
}
