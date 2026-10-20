// Rôle : Déclare les fonctions d'initialisation et de lecture des deux boutons.
// Interface GPIO des boutons, appelée par setup.c ; aucune notion de couleur ou de durée ici.
// Init prépare les entrées ; IsPressed renvoie l'état brut selon BUTTON_ACTIVE_LOW.
#ifndef GROVE_BUTTON_H
#define GROVE_BUTTON_H

#include <stdbool.h>

void GroveButton_Init(void);
bool GroveButton1_IsPressed(void);
bool GroveButton2_IsPressed(void);

#endif
