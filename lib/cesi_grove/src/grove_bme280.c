// Rôle : Relie le pilote Bosch BME280 aux échanges I2C de la carte STM32.
// Adaptateur maintenu pour le BME280 : relie src/environment.c au pilote Bosch tiers.
// Les fonctions de rappel ci-dessous traduisent les accès registres en transactions I2C HAL.
/* Interface HAL du pilote officiel Bosch, conservé avec sa licence. */
#include "bme280.h"
#include "grove_bme280.h"
#include "board_pins.h"
extern I2C_HandleTypeDef GROVE_I2C_HANDLE;
#include <math.h>
// dev conserve configuration et coefficients Bosch ; address reste valide pendant les fonctions de rappel.
static struct bme280_dev dev;
static uint8_t address;
// Fonction de rappel Bosch : lit n octets depuis reg vers data ; ctx pointe l'adresse I2C sur 7 bits.
// *(uint8_t *)ctx récupère cette adresse, << 1 l'adapte à la HAL ; 0 = succès, -1 = échec.
static int8_t read_reg(uint8_t reg, uint8_t *data, uint32_t n, void *ctx) {
    return HAL_I2C_Mem_Read(&hi2c1, (*(uint8_t *)ctx) << 1, reg, 1, data, n, 15) == HAL_OK ? 0 : -1;
}
// Fonction de rappel symétrique d'écriture ; data appartient à Bosch et n'est pas modifié ici.
// La conversion de type retire const pour la signature HAL ; elle ne permet pas de changer le contenu.
static int8_t write_reg(uint8_t reg, const uint8_t *data, uint32_t n, void *ctx) {
    return HAL_I2C_Mem_Write(&hi2c1, (*(uint8_t *)ctx) << 1, reg, 1, (uint8_t *)data, n, 15) ==
                   HAL_OK
               ? 0
               : -1;
}
// Délai demandé en microsecondes par Bosch, arrondi vers le haut en millisecondes HAL.
// Cette attente bloque ; l'acquisition laisse ensuite le temps de conversion via ses échéances.
static void delay_us(uint32_t us, void *ctx) {
    (void)ctx; /* Initialisation et configuration uniquement. */
    HAL_Delay((us + 999) / 1000);
}
// Essaie 0x76 puis 0x77, charge les coefficients et configure un échantillonnage simple.
// Vrai signifie initialisation/configuration réussie, pas validation électrique du module.
bool GroveBME280_Init(void) {
    dev = (struct bme280_dev){0};
    dev.intf = BME280_I2C_INTF;
    dev.read = read_reg;
    dev.write = write_reg;
    dev.delay_us = delay_us;
    dev.intf_ptr = &address;
    for (address = 0x76; address <= 0x77; address++) {
        if (bme280_init(&dev) != BME280_OK)
            continue;
        struct bme280_settings s = {0};
        s.osr_h = BME280_OVERSAMPLING_1X;
        s.osr_p = BME280_OVERSAMPLING_1X;
        s.osr_t = BME280_OVERSAMPLING_1X;
        s.filter = BME280_FILTER_COEFF_OFF;
        return bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &s, &dev) == BME280_OK;
    }
    return false;
}
// Déclenche une conversion unique en mode forcé ; la lecture sera faite plus tard par app.c.
bool GroveBME280_Begin(void) {
    return bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev) == BME280_OK;
}
// Remplit Env_Data avec les valeurs compensées ; faux si occupé, erreur I2C ou valeurs non finies.
// Appelée après conversion, hors IRQ ; seul un retour vrai autorise l'exploitation de *out.
bool GroveBME280_ReadEnvironment(Env_Data *out) {
    if (!out)
        return false;
    uint8_t status;
    // Le bit 3 du registre de statut indique une mesure encore en cours.
    if (read_reg(0xF3, &status, 1, &address) != 0 || (status & 8))
        return false;
    struct bme280_data data;
    if (bme280_get_sensor_data(BME280_ALL, &data, &dev) != BME280_OK)
        return false;
    out->temperature_c = data.temperature;
    out->humidity_percent = data.humidity;
    // Bosch fournit des pascals : diviser par 100 donne les hPa utilisés dans la station.
    out->pressure_hpa = data.pressure / 100.0;
    out->gas_resistance_ohm = NAN; /* Le BME280 ne mesure pas les gaz. */
    return isfinite(out->temperature_c) && isfinite(out->humidity_percent) &&
           isfinite(out->pressure_hpa);
}
