/* Implémentation réelle de SDLogger_WriteLine, absente de la base initiale.
 * Le volume doit être monté par Storage_Mount ; le fichier est fermé à la fin. */
#include "sd_logger.h"
#include "ff.h"
#include <string.h>
bool SDLogger_WriteLine(const char *filename, const char *line) {
    if (!filename || !line)
        return false;
    static FIL file;
    if (f_open(&file, filename, FA_WRITE | FA_OPEN_ALWAYS) != FR_OK)
        return false;
    bool ok = f_lseek(&file, f_size(&file)) == FR_OK;
    UINT written = 0;
    size_t size = strlen(line);
    if (ok)
        ok = f_write(&file, line, size, &written) == FR_OK && written == size;
    FRESULT closed = f_close(&file);
    return ok && closed == FR_OK;
}
