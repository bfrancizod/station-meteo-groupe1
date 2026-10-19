/* Initialisation Nucleo et E/S. Les IRQ ne font que déplacer des octets. */
#include "setup.h"
#include "board.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_gps_air530z.h"
#include "services.h"
#include <string.h>
#include <stdio.h>
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2, huart4;
#define RING_SIZE 1024u
typedef struct {
    volatile uint16_t head, tail;
    uint8_t data[RING_SIZE];
} Ring;
static Ring console_rx, gps_rx, console_tx;
static volatile uint32_t rx_lost;
static void put(Ring *r, uint8_t v) {
    uint16_t n = (r->head + 1u) % RING_SIZE;
    if (n == r->tail) {
        rx_lost++;
        return;
    }
    r->data[r->head] = v;
    r->head = n;
}
static int get(Ring *r) {
    if (r->head == r->tail)
        return -1;
    int v = r->data[r->tail];
    r->tail = (r->tail + 1u) % RING_SIZE;
    return v;
}
void SysTick_Handler(void) {
    HAL_IncTick();
}
/* Interruption courte : déplacer les octets et effacer les erreurs, sans décodage ni accès SD. */
static void irq(UART_HandleTypeDef *h, Ring *rx, bool tx) {
    USART_TypeDef *u = h->Instance;
    uint32_t s = u->ISR;
    if (s & USART_ISR_RXNE)
        put(rx, (uint8_t)u->RDR);
    if (s & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) {
        u->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
        rx_lost++;
    }
    if (tx && (s & USART_ISR_TXE) && (u->CR1 & USART_CR1_TXEIE)) {
        int c = get(&console_tx);
        if (c < 0)
            CLEAR_BIT(u->CR1, USART_CR1_TXEIE);
        else
            u->TDR = (uint8_t)c;
    }
}
void USART2_IRQHandler(void) {
    irq(&huart2, &console_rx, true);
}
void UART4_IRQHandler(void) {
    irq(&huart4, &gps_rx, false);
}
int Board_ConsoleGet(void) {
    return get(&console_rx);
}
int Board_GpsGet(void) {
    return get(&gps_rx);
}
uint32_t Board_RxOverflows(void) {
    return rx_lost;
}
void Board_ConsoleWrite(const char *s) {
    /* Ne jamais attendre le terminal : une sortie saturée est abandonnée. */
    while (*s) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        put(&console_tx, (uint8_t)*s++);
        SET_BIT(USART2->CR1, USART_CR1_TXEIE);
        __set_PRIMASK(mask);
    }
}
void Board_Pump(void) {
    App_Background();
}
bool Board_Red(void) {
    return RED_IS_BUTTON1 ? GroveButton1_IsPressed() : GroveButton2_IsPressed();
}
bool Board_Green(void) {
    return RED_IS_BUTTON1 ? GroveButton2_IsPressed() : GroveButton1_IsPressed();
}
void Board_SpiSpeed(bool fast) {
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR,
               fast ? SPI_BAUDRATEPRESCALER_8 : SPI_BAUDRATEPRESCALER_128);
    __HAL_SPI_ENABLE(&hspi1);
}
uint16_t Board_Light(void) {
    return GroveLight_ReadRaw();
}
/* Retarget printf conservé de setup.c, désormais avec émission tamponnée. */
int _write(int file, char *ptr, int len) {
    (void)file;
    for (int i = 0; i < len; i++) {
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        put(&console_tx, (uint8_t)ptr[i]);
        SET_BIT(USART2->CR1, USART_CR1_TXEIE);
        __set_PRIMASK(mask);
    }
    return len;
}
static void uart_init(UART_HandleTypeDef *h, USART_TypeDef *instance, uint32_t baud) {
    h->Instance = instance;
    h->Init.BaudRate = baud;
    h->Init.WordLength = UART_WORDLENGTH_8B;
    h->Init.StopBits = UART_STOPBITS_1;
    h->Init.Parity = UART_PARITY_NONE;
    h->Init.Mode = UART_MODE_TX_RX;
    h->Init.HwFlowCtl = UART_HWCONTROL_NONE;
    h->Init.OverSampling = UART_OVERSAMPLING_16;
    if (instance == UART4 && GPS_SWAP_PINS) {
        h->AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_SWAP_INIT;
        h->AdvancedInit.Swap = UART_ADVFEATURE_SWAP_ENABLE;
    }
    if (HAL_UART_Init(h) != HAL_OK)
        while (1) {
        }
    SET_BIT(instance->CR1, USART_CR1_RXNEIE);
    SET_BIT(instance->CR3, USART_CR3_EIE);
}
void Global_Init(void) {
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_MSI;
    osc.MSIState = RCC_MSI_ON;
    osc.MSIClockRange = RCC_MSIRANGE_8;
    osc.MSICalibrationValue = RCC_MSICALIBRATION_DEFAULT;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK)
        while (1) {
        }
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType =
        RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK)
        while (1) {
        }
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    GroveButton_Init();
    __HAL_RCC_I2C1_CLK_ENABLE();
    g.Pin = GPIO_PIN_8 | GPIO_PIN_9;
    g.Mode = GPIO_MODE_AF_OD;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF4_I2C1;
    HAL_GPIO_Init(GPIOB, &g);
    /* I2C alimenté par PCLK1=16 MHz ; environ 100 kHz, adapté au DS1307. */
    hi2c1.Instance = I2C1;
    hi2c1.Init.Timing = 0x00303D5B;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    HAL_I2C_Init(&hi2c1);
    HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE);
    __HAL_RCC_SPI1_CLK_ENABLE();
    g.Pin = GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF5_SPI1;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    g.Pin = SD_CS_PIN;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(SD_CS_PORT, &g);
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    hspi1.Init.Direction = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
    hspi1.Init.NSS = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_128;
    hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi1.Init.CRCPolynomial = 7;
    HAL_SPI_Init(&hspi1);
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();
    g.Pin = GPIO_PIN_2 | GPIO_PIN_3;
    g.Mode = GPIO_MODE_AF_PP;
    g.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;
    g.Pull = GPIO_PULLUP;
    g.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOA, &g);
    uart_init(&huart2, USART2, 115200);
    GroveGPS_Init();
    SET_BIT(UART4->CR1, USART_CR1_RXNEIE);
    SET_BIT(UART4->CR3, USART_CR3_EIE);
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(UART4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    setvbuf(stdout, NULL, _IONBF, 0); /* printf SD envoyé immédiatement au tampon UART. */
    GroveLight_Init();
}
