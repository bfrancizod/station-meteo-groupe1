#ifndef BOARD_H
#define BOARD_H
#include "board_pins.h"
#include <stdbool.h>
#include <stdint.h>
extern I2C_HandleTypeDef hi2c1;
extern SPI_HandleTypeDef hspi1;
extern UART_HandleTypeDef huart2, huart4;
void Global_Init(void);
void Board_Pump(void); /* Parse UART et rend le contrôle à l'interface. */
void Board_ConsoleWrite(const char *s);
int Board_ConsoleGet(void);
int Board_GpsGet(void);
uint32_t Board_RxOverflows(void);
bool Board_Red(void);
bool Board_Green(void);
uint16_t Board_Light(void);
void Board_SpiSpeed(bool fast);
#endif
