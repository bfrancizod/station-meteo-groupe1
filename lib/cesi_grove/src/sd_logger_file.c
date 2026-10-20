// Complément du pilote SD : ajoute une ligne via FatFs à la demande de storage.c.
// La gestion de la date et de la rotation reste dans storage.c, les secteurs dans diskio_spi.c.
/* Implémentation réelle de SDLogger_WriteLine, absente de la base initiale.
 * Le volume doit être monté par Storage_Mount ; le fichier est fermé à la fin. */
#include "sd_logger.h"
#include "ff.h"
#include <string.h>
// filename et line sont des chaînes terminées par zéro ; aucun saut de ligne n'est ajouté.
// Renvoie vrai seulement si positionnement, écriture complète et fermeture réussissent.
// Appelée dans la boucle principale ; le FIL statique n'autorise pas deux appels simultanés.
bool SDLogger_WriteLine(const char *filename, const char *line) {
    if (!filename || !line)
        return false;
    static FIL file;
    // Ouvre ou crée sans tronquer, puis se place en fin pour préserver les lignes précédentes.
    if (f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
        return false;
    bool ok = f_lseek(&file, f_size(&file)) == FR_OK;
    UINT written = 0;
    size_t size = strlen(line);
    if (ok)
        ok = f_write(&file, line, size, &written) == FR_OK && written == size;
    // f_close synchronise les données et métadonnées via FatFs ; il est appelé même
    // après une écriture ratée. Il n'y a pas d'appel f_sync séparé dans ce chemin.
    FRESULT closed = f_close(&file);
    return ok && closed == FR_OK;
}
