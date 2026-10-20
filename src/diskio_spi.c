// Rôle : Traduit les accès aux secteurs demandés par FatFs en échanges SD sur SPI.
// Adaptation de l'interface disque FatFs aux commandes SD sur SPI1.
// storage.c manipule des fichiers ; ce fichier ne connaît que des secteurs et des octets.
// L'initialisation s'appuie sur sd_logger.c et les broches/vitesses sur setup.c.
/* Couche secteurs SD SPI pour FatFs : SD v1/v2 et SDHC, secteurs 512 octets.
 * Aucune commande de formatage/effacement de volume n'est exécutée ici. */
#include "board.h"
#include "sd_logger.h"
#include "ff.h"
#include "diskio.h"
#include "services.h"
#include <string.h>
// state garde l'état d'initialisation ; block_addressed distingue adresse de secteur
// (SDHC) et adresse d'octet (SDSC). dummy fournit des 0xff pour générer les horloges de lecture.
static DSTATUS state = STA_NOINIT;
static bool block_addressed;
static uint32_t sectors;
static uint8_t dummy[512];
// Échange un octet SPI dans les deux sens ; une erreur HAL invalide l'état du disque.
static uint8_t byte(uint8_t b) {
    uint8_t r = 0xff;
    if (HAL_SPI_TransmitReceive(&hspi1, &b, &r, 1, 5) != HAL_OK)
        state = STA_NOINIT;
    return r;
}
// Relâche CS puis envoie huit horloges supplémentaires pour terminer l'échange.
static void release(void) {
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_SET);
    byte(0xff);
}
// Attend que la carte rende 0xff, dans la limite du délai fourni en millisecondes.
// Les tâches de fond restent servies pendant cette attente.
// La fonction reste synchrone : Board_Pump rend des services mais ne quitte pas cette attente.
static bool ready(uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    do {
        if (byte(0xff) == 0xff)
            return true;
        Board_Pump();
    } while (HAL_GetTick() - start < timeout);
    return false;
}
// Active CS (niveau bas) et attend la disponibilité ; faux relâche la carte.
static bool select_card(void) {
    HAL_GPIO_WritePin(SD_CS_PORT, SD_CS_PIN, GPIO_PIN_RESET);
    byte(0xff);
    if (ready(500))
        return true;
    release();
    return false;
}
// Envoie cmd et son argument 32 bits, puis attend la réponse courte R1.
// Renvoie 0xff en absence de réponse ; laisse CS actif pour lire des données supplémentaires.
static uint8_t command(uint8_t cmd, uint32_t arg) {
    release();
    if (!select_card())
        return 0xff;
    // Le paquet transmet l'argument octet fort d'abord ; CRC imposé pour CMD0 et CMD8.
    uint8_t packet[] = {0x40 | cmd, arg >> 24, arg >> 16,
                        arg >> 8,   arg,       cmd == 0 ? 0x95 : cmd == 8 ? 0x87 : 0x01};
    if (HAL_SPI_Transmit(&hspi1, packet, 6, 10) != HAL_OK)
        return 0xff;
    for (unsigned i = 0; i < 10; i++) {
        uint8_t r = byte(0xff);
        if (!(r & 0x80))
            return r;
    }
    return 0xff;
}
// Attend le jeton 0xfe puis lit n octets dans out ; ici n vaut 16 (CSD) ou 512 (secteur).
// out doit avoir cette capacité ; les deux octets CRC reçus ensuite sont consommés.
static bool receive(uint8_t *out, unsigned n) {
    uint8_t token;
    uint32_t start = HAL_GetTick();
    do {
        token = byte(0xff);
        if (token != 0xff)
            break;
        Board_Pump();
    } while (HAL_GetTick() - start < 300);
    if (token != 0xfe)
        return false;
    if (HAL_SPI_TransmitReceive(&hspi1, dummy, out, n, 100) != HAL_OK)
        return false;
    byte(0xff);
    byte(0xff);
    return true; /* CRC transmis mais non activé en mode SPI. */
}
// Point d'entrée FatFs pour le disque 0 : initialisation lente, lecture de capacité,
// puis vitesse de transfert normale. Renvoie 0 si prêt, STA_NOINIT sinon.
DSTATUS disk_initialize(BYTE drive) {
    if (drive)
        return STA_NOINIT;
    state = STA_NOINIT;
    block_addressed = false;
    sectors = 0;
    memset(dummy, 0xff, sizeof(dummy));
    Board_SpiSpeed(false);
    /* Le protocole d'initialisation reste celui du SDLogger du dépôt officiel. */
    if (!SDLogger_Init()) {
        release();
        return state;
    }
    block_addressed = SDLogger_GetCardType() == SDLOGGER_CARD_SDHC;
    // Une SDSC reçoit CMD16 pour fixer des blocs de 512 octets ; SDHC les impose déjà.
    if (!block_addressed && command(16, 512)) {
        release();
        return state;
    }
    // CMD9 lit le registre CSD : ses champs de bits codent la capacité.
    // Les masques gardent les bits utiles, les décalages reconstruisent les entiers.
    uint8_t csd[16];
    if (command(9, 0) || !receive(csd, 16)) {
        release();
        return state;
    }
    if ((csd[0] >> 6) == 1) {
        uint32_t size = ((uint32_t)(csd[7] & 63) << 16) | ((uint32_t)csd[8] << 8) | csd[9];
        sectors = (size + 1) * 1024u;
    } else {
        unsigned n = (csd[5] & 15) + ((csd[10] & 0x80) >> 7) + ((csd[9] & 3) << 1) + 2;
        uint32_t size =
            ((csd[8] >> 6) | ((uint32_t)csd[7] << 2) | ((uint32_t)(csd[6] & 3) << 10)) + 1;
        if (n < 9 || n > 31) {
            release();
            return state;
        }
        sectors = size << (n - 9);
    }
    release();
    if (!sectors)
        return state;
    state = 0;
    Board_SpiSpeed(true);
    return state;
}
// Renvoie l'état mémorisé ; cette consultation seule ne détecte pas physiquement un retrait.
DSTATUS disk_status(BYTE drive) {
    return drive ? STA_NOINIT : state;
}
// Lit count secteurs à partir de sector dans out (capacité requise : count * 512).
// CMD17 lit un bloc ; out avance de 512 octets après chaque transfert.
DRESULT disk_read(BYTE drive, BYTE *out, LBA_t sector, UINT count) {
    if (drive || !count)
        return RES_PARERR;
    if (state)
        return RES_NOTRDY;
    bool ok = true;
    while (count-- && ok) {
        ok = command(17, block_addressed ? sector : sector * 512u) == 0 && receive(out, 512);
        release();
        out += 512;
        sector++;
        Board_Pump();
    }
    if (!ok)
        state = STA_NOINIT;
    return ok ? RES_OK : RES_ERROR;
}
// Écrit count secteurs depuis data avec CMD24 ; data reste inchangé.
// RES_OK nécessite acceptation des blocs puis fin de l'occupation de la carte.
DRESULT disk_write(BYTE drive, const BYTE *data, LBA_t sector, UINT count) {
    if (drive || !count)
        return RES_PARERR;
    if (state)
        return RES_NOTRDY;
    bool ok = true;
    while (count-- && ok) {
        ok = command(24, block_addressed ? sector : sector * 512u) == 0;
        if (ok) {
            byte(0xff);
            byte(0xfe);
            ok = HAL_SPI_Transmit(&hspi1, (uint8_t *)data, 512, 100) == HAL_OK;
            byte(0xff);
            byte(0xff);
            uint8_t response = byte(0xff);
            // Les cinq bits faibles du jeton valent 00101 si le bloc a été accepté.
            ok = ok && (response & 31) == 5;
            if (ok)
                ok = ready(500);
        }
        release();
        data += 512;
        sector++;
        Board_Pump();
    }
    if (!ok)
        state = STA_NOINIT;
    return ok ? RES_OK : RES_ERROR;
}
// Commandes complémentaires demandées par FatFs ; buff est un pointeur de sortie
// dont le type dépend de cmd. CTRL_SYNC attend la fin des écritures matérielles.
DRESULT disk_ioctl(BYTE drive, BYTE cmd, void *buff) {
    if (drive)
        return RES_PARERR;
    if (state)
        return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC: {
        bool ok = select_card();
        if (ok)
            ok = ready(500);
        release();
        return ok ? RES_OK : RES_ERROR;
    }
    case GET_SECTOR_COUNT:
        // Convertir void* en LBA_t*, puis déréférencer, écrit dans la variable de FatFs.
        *(LBA_t *)buff = sectors;
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buff = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buff = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
// Fonction de rappel FatFs : compacte la date RTC dans 32 bits, avec une résolution de deux secondes.
// Renvoie zéro si la RTC est invalide ; l'horodatage du contenu du journal reste dans Measurement.
DWORD get_fattime(void) {
    DateTime d;
    if (!Time_Read(&d))
        return 0;
    return ((DWORD)(d.year - 1980) << 25) | ((DWORD)d.month << 21) | ((DWORD)d.day << 16) |
           ((DWORD)d.hour << 11) | ((DWORD)d.minute << 5) | (d.second / 2);
}
