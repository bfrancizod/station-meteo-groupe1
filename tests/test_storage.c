// Rôle : Vérifie les journaux et les archives FatFs sur une carte SD simulée en mémoire.
/* Intégration avec le vrai FatFs et un disque RAM : rotation, révisions >9,
 * reprise et refus d'une archive temporaire existante sans perte du fichier _0. */
#include "ff.h"
#include "diskio.h"
#include "services.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SECTORS 16384u
static unsigned char *disk;
DSTATUS disk_initialize(BYTE p) {
    return p ? STA_NOINIT : 0;
}
DSTATUS disk_status(BYTE p) {
    return disk_initialize(p);
}
DRESULT disk_read(BYTE p, BYTE *b, LBA_t s, UINT n) {
    if (p || s + n > SECTORS)
        return RES_PARERR;
    memcpy(b, disk + s * 512, n * 512);
    return RES_OK;
}
DRESULT disk_write(BYTE p, const BYTE *b, LBA_t s, UINT n) {
    if (p || s + n > SECTORS)
        return RES_PARERR;
    memcpy(disk + s * 512, b, n * 512);
    return RES_OK;
}
DRESULT disk_ioctl(BYTE p, BYTE cmd, void *b) {
    if (p)
        return RES_PARERR;
    switch (cmd) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        *(LBA_t *)b = SECTORS;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)b = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)b = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
DWORD get_fattime(void) {
    return (46u << 25) | (10u << 21) | (16u << 16);
}
void Board_Pump(void) {
}
bool Time_Read(DateTime *d) {
    return false;
}
int main(void) {
    disk = calloc(SECTORS, 512);
    assert(disk);
    BYTE work[4096];
    MKFS_PARM options = {.fmt = FM_FAT | FM_SFD, .n_fat = 1};
    assert(f_mkfs("", &options, work, sizeof(work)) == FR_OK);
    assert(Storage_Mount());
    Measurement m = {0};
    m.date = (DateTime){2026, 10, 16, 12, 0, 0, 6};
    m.date_valid = true;
    m.mode = MODE_STANDARD;
    for (unsigned i = 0; i < SENSOR_COUNT; i++)
        m.status[i] = VALUE_DISABLED;
    char line[256];
    int len = Measurement_Format(&m, line, sizeof(line));
    unsigned per_file = 512 / len;
    for (unsigned i = 0; i < per_file * 12 + 1; i++)
        assert(Storage_Write(&m, 512) == 0);
    FILINFO info;
    assert(f_stat("261016_10.LOG", &info) == FR_OK);
    assert(info.fsize <= 512);
    assert(f_stat("261016_12.LOG", &info) == FR_OK);
    Storage_Release();
    assert(Storage_Mount());
    assert(Storage_Write(&m, 512) == 0);
    /* Fichier actif préservé lorsqu'un temporaire empêche l'archivage suivant. */
    for (unsigned i = 2; i < per_file; i++)
        assert(Storage_Write(&m, 512) == 0);
    FIL tmp;
    assert(f_open(&tmp, "261016_13.TMP", FA_WRITE | FA_CREATE_NEW) == FR_OK);
    assert(f_close(&tmp) == FR_OK);
    assert(f_stat("261016_0.LOG", &info) == FR_OK);
    FSIZE_t before = info.fsize;
    assert(Storage_Write(&m, 512) == ERR_SD_IO);
    assert(f_stat("261016_0.LOG", &info) == FR_OK && info.fsize == before);
    m.date_valid = false;
    assert(Storage_Write(&m, 512) == ERR_RTC);
    Storage_Release();
    free(disk);
    puts("Integration FatFs RAM : archives, revision 10, reprise et conservation sur erreur : OK");
}
