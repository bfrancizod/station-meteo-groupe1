// Couche fichiers : app.c fournit une Measurement, format.c la transforme en texte,
// FatFs organise les fichiers et diskio_spi.c transfère les secteurs à la carte SD.
// SDLogger_Init réussi, volume monté et ligne écrite sont trois étapes distinctes.
/* Journal FAT. Une archive n'efface jamais le fichier actif avant vérification.
 * Une panne électrique peut néanmoins endommager FAT : ce n'est pas un FS journalisé. */
#include "board.h"
#include "sd_logger.h"
#include "ff.h"
#include "services.h"
#include <stdio.h>
#include <string.h>
// Objets persistants pour FatFs et buffers de copie de 512 octets, privés au fichier.
// Le code suppose un seul appel de stockage à la fois ; Board_Pump n'en déclenche pas d'autre.
static FATFS fs;
static bool mounted;
static FIL active, archive, check;
static uint8_t buffer[512], verify[512];
// Désenregistre le volume pour la maintenance ; les chemins d'écriture ferment leurs fichiers.
// Cette fonction ne ferme pas elle-même un fichier resté ouvert et ne coupe pas l'alimentation.
void Storage_Release(void) {
    f_mount(0, "", 0);
    mounted = false;
}
// Force une nouvelle lecture du volume FAT ; vrai signifie montage réussi, pas écriture testée.
bool Storage_Mount(void) {
    Storage_Release();
    mounted = f_mount(&fs, "", 1) == FR_OK;
    return mounted;
}
// Classe un échec : SD pleine seulement si FatFs confirme zéro cluster libre.
// Un cluster est un groupe de secteurs alloué à un fichier ; sinon conserver l'erreur d'accès.
static uint32_t error_kind(void) {
    DWORD free_clusters;
    FATFS *volume;
    if (f_getfree("", &free_clusters, &volume) == FR_OK && free_clusters == 0)
        return ERR_SD_FULL;
    return ERR_SD_IO;
}
/* Copier vers un temporaire puis comparer les octets avant de réutiliser le fichier actif. */
// name désigne le journal actif ; vrai signifie copie relue, fermée et renommée avec succès.
// La recherche de révision évite d'écraser une archive existante du même jour.
static bool archive_file(const char *name) {
    char target[32], tmp[32];
    FILINFO info;
    unsigned revision;
    for (revision = 1; revision < 100000; revision++) {
        snprintf(target, sizeof(target), "%.6s_%u.LOG", name, revision);
        FRESULT r = f_stat(target, &info);
        if (r == FR_NO_FILE)
            break;
        if (r != FR_OK)
            return false;
    }
    if (revision == 100000)
        return false;
    snprintf(tmp, sizeof(tmp), "%.6s_%u.TMP", name, revision);
    /* Ne pas écraser un temporaire de la session précédente : demander intervention. */
    // FA_CREATE_NEW refuse un .TMP déjà présent, par exemple après une coupure.
    // Il reste disponible pour inspection ; aucune suppression automatique n'est faite ici.
    if (f_open(&archive, tmp, FA_WRITE | FA_CREATE_NEW) != FR_OK)
        return false;
    if (f_open(&active, name, FA_READ) != FR_OK) {
        f_close(&archive);
        return false;
    }
    bool ok = true;
    UINT readn, written;
    // readn et written sont remplis par adresse ; un succès FatFs avec écriture courte
    // ne suffit pas. La copie se fait par blocs pour ne pas charger tout le fichier en RAM.
    do {
        if (f_read(&active, buffer, sizeof(buffer), &readn) != FR_OK) {
            ok = false;
            break;
        }
        if (readn && (f_write(&archive, buffer, readn, &written) != FR_OK || written != readn)) {
            ok = false;
            break;
        }
        Board_Pump();
    } while (readn);
    // Fermer l'archive force aussi la synchronisation FatFs avant sa relecture.
    FRESULT closea = f_close(&active), closeb = f_close(&archive);
    ok = ok && closea == FR_OK && closeb == FR_OK;
    if (!ok)
        return false;
    /* Relecture comparée avant renommage et avant réutilisation de _0. */
    if (f_open(&active, name, FA_READ) != FR_OK)
        return false;
    if (f_open(&check, tmp, FA_READ) != FR_OK) {
        f_close(&active);
        return false;
    }
    do {
        UINT n2;
        if (f_read(&active, buffer, sizeof(buffer), &readn) != FR_OK ||
            f_read(&check, verify, sizeof(verify), &n2) != FR_OK || readn != n2 ||
            memcmp(buffer, verify, readn)) {
            ok = false;
            break;
        }
        Board_Pump();
    } while (readn);
    closea = f_close(&active);
    closeb = f_close(&check);
    ok = ok && closea == FR_OK && closeb == FR_OK;
    return ok && f_rename(tmp, target) == FR_OK;
}
// Ajoute la mesure m au journal du jour ; max_size est la limite en octets.
// Renvoie 0 en cas de succès, sinon un masque ERR_* ; appelée hors interruption.
uint32_t Storage_Write(const Measurement *m, uint32_t max_size) {
    // Refuser une date invalide avant tout montage : ERR_RTC n'est pas une panne de SD.
    if (!m->date_valid)
        return ERR_RTC;
    if (!mounted && !Storage_Mount())
        return ERR_SD_IO;
    char line[256], name[32];
    int n = Measurement_Format(m, line, sizeof(line));
    if (n < 0 || (uint32_t)n > max_size)
        return ERR_SD_IO;
    // Le nom utilise les deux derniers chiffres de l'année : 20/10/2026 -> 261020_0.LOG.
    snprintf(name, sizeof(name), "%02u%02u%02u_0.LOG", m->date.year - 2000, m->date.month,
             m->date.day);
    FILINFO info;
    FRESULT r = f_stat(name, &info);
    if (r != FR_OK && r != FR_NO_FILE)
        return ERR_SD_IO;
    // Rotation avant l'ajout qui dépasserait la limite ; l'égalité exacte reste acceptée.
    // Le fichier actif n'est tronqué qu'après la copie et sa vérification intégrale.
    if (r == FR_OK && info.fsize + (unsigned)n > max_size) {
        if (!archive_file(name))
            return error_kind();
        if (f_open(&active, name, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK)
            return error_kind();
        if (f_close(&active) != FR_OK)
            return ERR_SD_IO;
    }
    /* Utiliser la fonction de fichier prévue par la base, désormais complétée. */
    bool ok = SDLogger_WriteLine(name, line);
    if (ok)
        return 0;
    uint32_t e = error_kind();
    Storage_Release();
    return e;
}
