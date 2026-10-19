#include "grove_bme680.h"
#include "board_pins.h"

extern I2C_HandleTypeDef GROVE_I2C_HANDLE;

#define BME680_ADDR_DEFAULT (0x76 << 1)
#define BME680_REG_ID      0xD0
#define BME680_REG_RESET   0xE0

static uint16_t bme680_addr = BME680_ADDR_DEFAULT;

static bool read_reg(uint8_t reg, uint8_t *data, uint16_t len)
{
    if (HAL_I2C_Master_Transmit(&GROVE_I2C_HANDLE, bme680_addr, &reg, 1, 100) != HAL_OK)
        return false;

    return HAL_I2C_Master_Receive(&GROVE_I2C_HANDLE, bme680_addr, data, len, 100) == HAL_OK;
}

/* Écriture de registres non utilisée : le BME680 reste un pilote hérité. */

bool GroveBME680_ReadChipId(uint8_t *chip_id)
{
    if (chip_id == 0) return false;
    return read_reg(BME680_REG_ID, chip_id, 1);
}

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

bool GroveBME680_ReadEnvironment(Env_Data *data)
{
    (void)data;
    /* Pilote hérité non utilisé sur cette maquette : compensation BME680 non
     * implémentée dans le dépôt initial. Ne jamais renvoyer de fausses mesures.
     * Le vrai BME280 est géré séparément par GroveBME280_ReadEnvironment. */
    return false;
}
