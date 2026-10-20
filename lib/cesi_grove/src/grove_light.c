// Pilote de luminosité analogique sur PA4/ADC1_IN9, appelé via Board_Light dans setup.c.
// Le pilote fournit une valeur brute ; app.c la ramène à 0..1023, sans conversion en lux.
#include "grove_light.h"
#include "board_pins.h"

ADC_HandleTypeDef hadc1;

// Initialise et calibre ADC1 pour une seule conversion déclenchée par logiciel.
// Appelée au démarrage ; le mode analogique évite d'utiliser cette broche comme entrée numérique.
void GroveLight_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_ADC_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};
    gpio.Pin = LIGHT_GPIO_PIN;
    gpio.Mode = GPIO_MODE_ANALOG_ADC_CONTROL;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_GPIO_PORT, &gpio);

    hadc1.Instance = ADC1;
    hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
    hadc1.Init.Resolution = ADC_RESOLUTION_12B;
    hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
    hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
    hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
    hadc1.Init.LowPowerAutoWait = DISABLE;
    hadc1.Init.ContinuousConvMode = DISABLE;
    hadc1.Init.NbrOfConversion = 1;
    hadc1.Init.DiscontinuousConvMode = DISABLE;
    hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
    hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
    hadc1.Init.DMAContinuousRequests = DISABLE;
    hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
    hadc1.Init.OversamplingMode = DISABLE;

    HAL_ADC_Init(&hadc1);
    HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED);

    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = LIGHT_ADC_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_47CYCLES_5;
    sConfig.SingleDiff = ADC_SINGLE_ENDED;
    sConfig.OffsetNumber = ADC_OFFSET_NONE;
    sConfig.Offset = 0;

    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

// Démarre puis attend au plus 5 ms une conversion ; renvoie 0..4095 ou 0xffff en échec.
// 0xffff est hors de la plage 12 bits et ne doit jamais être pris pour une luminosité.
// Ce court appel est bloquant ; les nouvelles tentatives sont espacées par app.c.
uint16_t GroveLight_ReadRaw(void)
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
        return 0xFFFF;

    if (HAL_ADC_PollForConversion(&hadc1, 5) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return 0xFFFF;
    }

    uint16_t value = (uint16_t)HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Stop(&hadc1);

    return value;
}