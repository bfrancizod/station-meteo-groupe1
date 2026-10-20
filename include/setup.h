// Rôle : Déclare la fonction qui prépare le matériel au démarrage.
// Point d'initialisation conservé de la base : main.c l'appelle avant les services applicatifs.
// L'implémentation de setup.c configure STM32, communications, boutons et ADC.
#ifndef SETUP_H
#define SETUP_H

// Aucun paramètre ni résultat ; à appeler une fois au démarrage avant App_Init.
void Global_Init(void);

#endif
