#!/usr/bin/env bash
# Rôle : Téléverse un binaire autorisé avec OpenOCD sans effacer les pages de réglages.
# Charge le binaire validé à l'adresse Flash normale, sans effacement complet.
# Les pages de paramètres à la fin de la Flash ne doivent pas être effacées.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
firmware="${1:-dist/firmware.bin}"
if ! command -v openocd >/dev/null 2>&1; then echo 'Installer OpenOCD ou utiliser PlatformIO Upload.' >&2; exit 1; fi
# Le chemin du binaire est transmis comme argument Tcl, sans évaluation de commande shell.
if [[ "$firmware" != dist/firmware.bin && "$firmware" != build/firmware.bin ]]; then
 echo 'Chemins autorisés : dist/firmware.bin ou build/firmware.bin' >&2; exit 1
fi
exec openocd -f interface/stlink.cfg -f target/stm32l4x.cfg \
 -c "program {$firmware} verify reset exit 0x08000000"
