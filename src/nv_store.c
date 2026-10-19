/* Deux pages Flash réservées par linker.ld : l'ancienne reste valide si
 * l'alimentation disparaît pendant l'écriture de la nouvelle. Pas d'EEPROM externe. */
#include "board.h"
#include "services.h"
#include <stddef.h>
#include <string.h>
#define SLOT0 0x080FF000u
#define SLOT1 0x080FF800u
#define COMMIT 0x4345534957575731ULL
/* Format propre à la version 1 du firmware ; augmenter version si Config change. */
typedef struct {
    uint32_t version, sequence;
    Config cfg;
    uint32_t crc, reserved;
    uint64_t commit;
} Record;
_Static_assert(sizeof(Record) % 8 == 0, "Alignement Flash doubleword");
static bool valid(const Record *r) {
    return r->commit == COMMIT && r->version == 1 && r->crc == Crc32(r, offsetof(Record, crc)) &&
           Config_Validate(&r->cfg);
}
static const Record *latest(void) {
    const Record *a = (const Record *)SLOT0, *b = (const Record *)SLOT1;
    if (!valid(a))
        return valid(b) ? b : 0;
    if (!valid(b))
        return a;
    return (int32_t)(b->sequence - a->sequence) > 0 ? b : a;
}
bool Nv_Load(Config *c) {
    const Record *r = latest();
    if (!r)
        return false;
    *c = r->cfg;
    return true;
}
bool Nv_Save(const Config *c) {
    if (!Config_Validate(c))
        return false;
    /* Sans DBANK, les pages changent de taille : refuser plutôt qu'effacer du code. */
    if (!(FLASH->OPTR & FLASH_OPTR_DUALBANK) || (FLASH->OPTR & FLASH_OPTR_BFB2))
        return false;
    const Record *old = latest();
    if (old && !memcmp(c, &old->cfg, sizeof(*c)))
        return true;
    uint32_t dst = old == (const Record *)SLOT0 ? SLOT1 : SLOT0;
    Record next;
    memset(&next, 0, sizeof(next));
    next.version = 1;
    next.sequence = old ? old->sequence + 1 : 1;
    next.cfg = *c;
    next.crc = Crc32(&next, offsetof(Record, crc));
    next.commit = COMMIT;
    FLASH_EraseInitTypeDef erase = {0};
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_2;
    erase.Page = dst == SLOT0 ? 254 : 255;
    erase.NbPages = 1;
    uint32_t error;
    bool ok = HAL_FLASH_Unlock() == HAL_OK;
    if (ok)
        ok = HAL_FLASHEx_Erase(&erase, &error) == HAL_OK;
    for (unsigned i = 0; ok && i < sizeof(next); i += 8) {
        uint64_t word;
        memcpy(&word, (uint8_t *)&next + i, 8);
        ok = HAL_FLASH_Program(FLASH_TYPEPROGRAM_DOUBLEWORD, dst + i, word) == HAL_OK;
    }
    HAL_FLASH_Lock();
    return ok && valid((const Record *)dst);
}
