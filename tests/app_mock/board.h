#ifndef APP_MOCK_BOARD_H
#define APP_MOCK_BOARD_H
#include <stdint.h>
#include <stdbool.h>
#define GPIOA ((void*)1)
#define GPIOB ((void*)2)
#define GPIO_PIN_10 1024
#define GPIO_PIN_3 8
#define HAL_OK 0
extern int hi2c1;
uint32_t HAL_GetTick(void);
unsigned HAL_GPIO_ReadPin(void *port,unsigned pin);
int HAL_I2C_IsDeviceReady(void *bus,unsigned address,unsigned trials,unsigned timeout);
void Board_ConsoleWrite(const char *s);
int Board_ConsoleGet(void);
bool Board_Red(void);
bool Board_Green(void);
uint32_t Board_RxOverflows(void);
uint16_t Board_Light(void);
#endif
