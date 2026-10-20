// Rôle : Déclare les fonctions de lecture du capteur de luminosité.
// Interface ADC utilisée par Board_Light dans setup.c, puis par app.c.
// Init calibre ADC1 ; ReadRaw renvoie 0..4095 ou 0xffff si conversion impossible.
#ifndef GROVE_LIGHT_H
#define GROVE_LIGHT_H

#include <stdint.h>

void GroveLight_Init(void);
uint16_t GroveLight_ReadRaw(void);

#endif
