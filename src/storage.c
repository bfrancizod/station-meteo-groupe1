/* Journal FAT. Une archive n'efface jamais le fichier actif avant vérification.
 * Une panne électrique peut néanmoins endommager FAT : ce n'est pas un FS journalisé. */
#include "board.h"
#include "sd_logger.h"
#include "ff.h"
#include "services.h"
#include <stdio.h>
#include <string.h>
static FATFS fs;
static bool mounted;
static FIL active, archive, check;
static uint8_t buffer[512], verify[512];
void Storage_Release(void) {
    f_mount(0, "", 0);
    mounted = false;
}
bool Storage_Mount(void) {
    Storage_Release();
    mounted = f_mount(&fs, "", 1) == FR_OK;
    return mounted;
}
static uint32_t error_kind(void) {
    DWORD free_clusters;
    FATFS *volume;
    if (f_getfree("", &free_clusters, &volume) == FR_OK && free_clusters == 0)
        return ERR_SD_FULL;
    return ERR_SD_IO;
}
/* Copier vers un temporaire puis comparer les octets avant de réutiliser le fichier actif. */
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
    if (f_open(&archive, tmp, FA_WRITE | FA_CREATE_NEW) != FR_OK)
        return false;
    if (f_open(&active, name, FA_READ) != FR_OK) {
        f_close(&archive);
        return false;
    }
    bool ok = true;
    UINT readn, written;
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
uint32_t Storage_Write(const Measurement *m, uint32_t max_size) {
    if (!m->date_valid)
        return ERR_RTC;
    if (!mounted && !Storage_Mount())
        return ERR_SD_IO;
    char line[256], name[32];
    int n = Measurement_Format(m, line, sizeof(line));
    if (n < 0 || (uint32_t)n > max_size)
        return ERR_SD_IO;
    snprintf(name, sizeof(name), "%02u%02u%02u_0.LOG", m->date.year - 2000, m->date.month,
             m->date.day);
    FILINFO info;
    FRESULT r = f_stat(name, &info);
    if (r != FR_OK && r != FR_NO_FILE)
        return ERR_SD_IO;
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
