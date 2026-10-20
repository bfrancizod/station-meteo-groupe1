// Couche carte : configure les périphériques STM32 puis fournit les E/S de board.h.
// main.c appelle Global_Init ; les IRQ remplissent des buffers consommés par app.c et le GPS.
/* Initialisation Nucleo et E/S. Les IRQ ne font que déplacer des octets. */
#include "setup.h"
#include "board.h"
#include "grove_button.h"
#include "grove_light.h"
#include "grove_gps_air530z.h"
#include "services.h"
#include <string.h>
#include <stdio.h>
// Les handles sont des structures HAL décrivant les périphériques ; board.h les déclare extern.
I2C_HandleTypeDef hi2c1;
SPI_HandleTypeDef hspi1;
UART_HandleTypeDef huart2, huart4;
#define RING_SIZE 1024u
// Buffer circulaire : head pointe la prochaine écriture, tail la prochaine lecture.
// uint8_t = entier non signé sur 8 bits (un octet) ; uint16_t sur 16 bits pour les indices.
// Une case reste libre pour distinguer plein et vide : capacité utile = 1023 octets.
typedef struct {
    // volatile oblige à relire/publier les indices susceptibles de changer dans une IRQ.
    // Il ne rend pas une suite d'opérations atomique et ne remplace pas une protection.
    volatile uint16_t head, tail;
    uint8_t data[RING_SIZE];
} Ring;
static Ring console_rx, gps_rx, console_tx;
// Le nom historique rx_lost inclut aussi les abandons TX et certaines erreurs UART.
static volatile uint32_t rx_lost;
// Ajoute v au buffer pointé par r ; si plein, perd le nouvel octet et compte cette perte.
// Appelée par les IRQ RX ou sous protection des interruptions pour l'émission console.
static void put(Ring *r, uint8_t v) {
    uint16_t n = (r->head + 1u) % RING_SIZE;
    if (n == r->tail) {
        rx_lost++;
        return;
    }
    r->data[r->head] = v;
    r->head = n;
}
// Retire un octet, ou renvoie -1 si vide ; int permet de distinguer -1 des octets 0..255.
// Le modulo fait revenir tail au début du tableau après la dernière case.
static int get(Ring *r) {
    if (r->head == r->tail)
        return -1;
    int v = r->data[r->tail];
    r->tail = (r->tail + 1u) % RING_SIZE;
    return v;
}
// Interruption périodique de la base de temps HAL ; incrémente le compteur utilisé en ms.
void SysTick_Handler(void) {
    HAL_IncTick();
}
/* Interruption courte : déplacer les octets et effacer les erreurs, sans décodage ni accès SD. */
// h désigne l'UART, rx son buffer de réception, tx autorise le traitement d'émission.
// -> accède aux registres du périphérique ; & teste les bits d'état et | combine les masques.
static void irq(UART_HandleTypeDef *h, Ring *rx, bool tx) {
    USART_TypeDef *u = h->Instance;
    uint32_t s = u->ISR;
    // RXNE annonce un octet lisible ; lire RDR le retire du registre matériel.
    if (s & USART_ISR_RXNE)
        put(rx, (uint8_t)u->RDR);
    if (s & (USART_ISR_ORE | USART_ISR_NE | USART_ISR_FE | USART_ISR_PE)) {
        u->ICR = USART_ICR_ORECF | USART_ICR_NCF | USART_ICR_FECF | USART_ICR_PECF;
        rx_lost++;
    }
    // TXE signifie que TDR peut accepter un octet, pas que toute la chaîne a été envoyée.
    // Quand le buffer est vide, couper TXEIE évite des interruptions inutiles en continu.
    if (tx && (s & USART_ISR_TXE) && (u->CR1 & USART_CR1_TXEIE)) {
        int c = get(&console_tx);
        if (c < 0)
            CLEAR_BIT(u->CR1, USART_CR1_TXEIE);
        else
            u->TDR = (uint8_t)c;
    }
}
// Vecteur d'interruption USART2 : console à 115200 bauds, réception et émission.
void USART2_IRQHandler(void) {
    irq(&huart2, &console_rx, true);
}
// Vecteur d'interruption UART4 : réception GPS à 9600 bauds ; aucun décodage NMEA ici.
void UART4_IRQHandler(void) {
    irq(&huart4, &gps_rx, false);
}
// Lecture non bloquante d'un octet de console, ou -1 si aucun n'est disponible.
int Board_ConsoleGet(void) {
    return get(&console_rx);
}
// Même contrat pour les octets GPS, que GroveGPS_Process assemble en lignes.
int Board_GpsGet(void) {
    return get(&gps_rx);
}
// Renvoie le cumul des pertes/erreurs observées, sans remettre le compteur à zéro.
uint32_t Board_RxOverflows(void) {
    return rx_lost;
}
// Copie la chaîne s dans le buffer TX ; les interruptions assureront son émission.
void Board_ConsoleWrite(const char *s) {
    /* Ne jamais attendre le terminal : une sortie saturée est abandonnée. */
    while (*s) {
        // Sauvegarder puis restaurer PRIMASK conserve l'état antérieur des interruptions.
        // La section protégée empêche l'IRQ TX d'intervenir au milieu de l'ajout d'un octet.
        uint32_t mask = __get_PRIMASK();
        __disable_irq();
        put(&console_tx, (uint8_t)*s++);
        SET_BIT(USART2->CR1, USART_CR1_TXEIE);
        __set_PRIMASK(mask);
    }
}
// Point de coopération des attentes SD : maintient GPS, boutons et LED disponibles.
// Ne lance ni commande de console ni nouvelle opération FatFs imbriquée.
void Board_Pump(void) {
    App_Background();
}
// Applique l'association de couleurs du montage : RED_IS_BUTTON1 vaut ici 0.
bool Board_Red(void) {
    return RED_IS_BUTTON1 ? GroveButton1_IsPressed() : GroveButton2_IsPressed();
}
// Renvoie l'autre bouton physique comme bouton vert, avec la même polarité Grove.
bool Board_Green(void) {
    return RED_IS_BUTTON1 ? GroveButton2_IsPressed() : GroveButton1_IsPressed();
}
// fast choisit le diviseur SPI : lent pendant l'initialisation SD, plus rapide ensuite.
// Le périphérique est désactivé avant de modifier seulement les bits du prescaler.
void Board_SpiSpeed(bool fast) {
    __HAL_SPI_DISABLE(&hspi1);
    MODIFY_REG(hspi1.Instance->CR1, SPI_CR1_BR,
               fast ? SPI_BAUDRATEPRESCALER_8 : SPI_BAUDRATEPRESCALER_128);
    __HAL_SPI_ENABLE(&hspi1);
}
// Renvoie l'ADC brut 0..4095, ou 0xffff si le pilote n'a pas obtenu de conversion.
uint16_t Board_Light(void) {
    return GroveLight_ReadRaw();
}
/* Retarget printf conservé de setup.c, désormais avec émission tamponnée. */
// Adaptateur appelé par printf/newlib : ptr contient len octets, sans exigence de '\0'.
// Renvoie len même si le buffer a saturé ; ce retour n'est pas un accusé de réception du PC.
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
// Configure le handle h pour instance à baud bits/s, en 8 bits sans parité et un bit d'arrêt.
// Active la réception par interruption ; une erreur HAL d'initialisation arrête cette boucle.
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
// Appel unique au démarrage : horloges, GPIO, I2C, SPI, UART et ADC.
// Les structures locales {0} commencent à zéro ; &g transmet leur adresse aux fonctions HAL.
void Global_Init(void) {
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
    // MSI interne réglé sur 16 MHz, sans PLL ; les bus AHB/APB gardent cette fréquence.
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
    // PB8/PB9 : fonction alternative I2C1, sorties drain ouvert (la ligne est relâchée
    // pour l'état haut). Cette configuration ne valide pas les tensions du montage réel.
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
    // PA5/PA6/PA7 portent SCK/MISO/MOSI ; PB5 est le CS logiciel, actif au niveau bas.
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
    // PA2/PA3 : console USART2 ; PA0/PA1 : GPS UART4 avec permutation dans GroveGPS_Init.
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
    // Une valeur de priorité plus petite donne ici la priorité à l'IRQ GPS.
    HAL_NVIC_SetPriority(USART2_IRQn, 2, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
    HAL_NVIC_SetPriority(UART4_IRQn, 1, 0);
    HAL_NVIC_EnableIRQ(UART4_IRQn);
    setvbuf(stdout, NULL, _IONBF, 0); /* printf SD envoyé immédiatement au tampon UART. */
    GroveLight_Init();
}
