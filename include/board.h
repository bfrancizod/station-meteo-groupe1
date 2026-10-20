// Interface matérielle implémentée par setup.c, partagée avec app.c et les pilotes.
// Les gardes #ifndef/#define empêchent de déclarer plusieurs fois le même en-tête.
#ifndef BOARD_H
#define BOARD_H
#include "board_pins.h"
#include <stdbool.h>
#include <stdint.h>
// extern annonce des objets définis ailleurs ; ces lignes ne créent pas de nouveaux périphériques.
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2, huart4;
// Initialisation unique avant App_Init ; configure horloges, broches et communications.
void Global_Init(void);
// Le fond traite GPS, boutons et LED ; les commandes console restent dans App_Process.
void Board_Pump(void); /* Parse UART et rend le contrôle à l'interface. */
// Enfile une chaîne terminée par zéro ; des octets peuvent être abandonnés si le tampon est plein.
void Board_ConsoleWrite(const char *s);
// Lectures sans attente : un octet 0..255, ou -1 si le tampon correspondant est vide.
int Board_ConsoleGet(void);
int Board_GpsGet(void);
// Compte aussi les pertes d'émission et erreurs UART, malgré le nom historique RxOverflows.
uint32_t Board_RxOverflows(void);
// États instantanés selon les couleurs configurées ; filtrage effectué ensuite par app.c.
bool Board_Red(void);
bool Board_Green(void);
// ADC brut 12 bits, ou 0xffff en échec ; fast choisit ensuite le diviseur de l'horloge SPI.
uint16_t Board_Light(void);
void Board_SpiSpeed(bool fast);
#endif
