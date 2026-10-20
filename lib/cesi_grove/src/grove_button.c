// Rôle : Lit l'état électrique des deux boutons selon leur polarité configurée.
// Lecture GPIO des deux boutons, utilisée par les adaptateurs Board_Red/Board_Green de setup.c.
// L'association aux couleurs vient de board_pins.h ; rebonds et appuis longs sont traités dans app.c.
#include "grove_button.h"
#include "board_pins.h"

// Prépare PA10 et PB3 comme entrées ; le rappel impose un niveau de repos quand le contact est ouvert.
void GroveButton_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Mode = GPIO_MODE_INPUT;
    /* La polarité dépend du module ; le rappel électrique doit suivre le niveau de repos. */
    gpio.Pull = BUTTON_ACTIVE_LOW ? GPIO_PULLUP : GPIO_PULLDOWN;

    gpio.Pin = BUTTON1_PIN;
    HAL_GPIO_Init(BUTTON1_PORT, &gpio);

    gpio.Pin = BUTTON2_PIN;
    HAL_GPIO_Init(BUTTON2_PORT, &gpio);
}

// Renvoie l'état instantané du bouton 1, vrai si pressé selon BUTTON_ACTIVE_LOW.
// Ce résultat n'est pas encore filtré contre les rebonds mécaniques.
bool GroveButton1_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(BUTTON1_PORT, BUTTON1_PIN) == GPIO_PIN_RESET) == BUTTON_ACTIVE_LOW;
}

// Même lecture pour le bouton 2 ; dans ce montage, Board_Red utilise ce bouton.
bool GroveButton2_IsPressed(void)
{
    return (HAL_GPIO_ReadPin(BUTTON2_PORT, BUTTON2_PIN) == GPIO_PIN_RESET) == BUTTON_ACTIVE_LOW;
}