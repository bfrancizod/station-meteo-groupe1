// Affectations utilisées par setup.c et les pilotes Grove pour le montage de cette station.
// Les macros remplacent des noms à la compilation ; elles ne contrôlent pas le câblage physique.
// Une communication observée ne suffit pas à valider les tensions admissibles des modules.
#ifndef BOARD_PINS_H
#define BOARD_PINS_H

#include "stm32l4xx_hal.h"

// La LED reçoit une trame série P9813 à horloge sur son entrée IN ; ce n'est pas de l'I2C.
/*
 * Grove Chainable RGB LED :
 * - Port Grove D6 du shield
 * - Signal1=D6=PB10 → CIN (horloge P9813)
 * - Signal2=D7=PA8  → DIN (données P9813)
 */
#define RGB_DATA_PORT   GPIOA
#define RGB_DATA_PIN    GPIO_PIN_8

#define RGB_CLK_PORT    GPIOB
#define RGB_CLK_PIN     GPIO_PIN_10

/*
 * Grove Light Sensor v1.3
 * Port Grove A2 du shield (PA4 = ADC1_IN9).
 */
#define LIGHT_ADC_CHANNEL  ADC_CHANNEL_9
#define LIGHT_GPIO_PORT    GPIOA
#define LIGHT_GPIO_PIN     GPIO_PIN_4

/*
 * Grove Dual Button v1.0
 * À brancher sur un port digital Grove, par exemple D2.
 * À adapter selon le port choisi et le mapping réel.
 */
#define BUTTON1_PORT GPIOA
#define BUTTON1_PIN  GPIO_PIN_10

#define BUTTON2_PORT GPIOB
#define BUTTON2_PIN  GPIO_PIN_3

/*
 * Grove GPS Air530Z / SIM28
 * Port Grove A0 du shield (PA0=UART4_TX, PA1=UART4_RX, AF8).
 * Seul UART libre sur un connecteur Grove unique (USART2=debug, USART1 split sur D2+D8).
 */

/*
 * Grove RTC DS1307 + BME280 sur la maquette (BME680 dans la base)
 * Tous deux sur I2C.
 */
// Le handle I2C1 est configuré sur PB8/SCL et PB9/SDA dans setup.c.
// Les pilotes essaient 0x68 pour la RTC et 0x76/0x77 pour le BME280.
#define GROVE_I2C_HANDLE hi2c1

/*
 * SD Card Shield Seeed v4.3
 * SPI Arduino : D11/D12/D13 et CS sur D4.
 */
#define SD_CS_PORT GPIOB
#define SD_CS_PIN  GPIO_PIN_5

// SPI1 utilise PA5/SCK, PA6/MISO, PA7/MOSI ; le CS PB5 est commandé séparément.
#define SD_SPI_HANDLE hspi1

/* Options du montage réel : les broches de la base restent inchangées. */
// 0 associe rouge au bouton 2 (PB3), vert au bouton 1 (PA10) : correction du montage actuel.
#define RED_IS_BUTTON1 0
// 1 signifie appui au niveau bas ; les GPIO utilisent donc un rappel au niveau haut.
#define BUTTON_ACTIVE_LOW 1
// 1 active l'échange RX/TX UART4 sur PA0/PA1 dans le pilote GPS.
#define GPS_SWAP_PINS 1
#define GPS_UART_HANDLE huart4

#endif