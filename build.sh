#!/usr/bin/env bash
# Rôle : Lance la compilation, les tests, le téléversement ou la console selon l'option choisie.
# Utilisation : ./build.sh build|upload|monitor|test|direct
set -euo pipefail
cd -- "$(dirname -- "$0")"
action="${1:-build}"
case "$action" in
 test) exec bash scripts/test_host.sh ;;
 direct) exec python3 scripts/build_direct.py ;;
 build|upload|monitor)
  if command -v pio >/dev/null 2>&1; then pio_cmd=(pio)
  elif python3 -c 'import platformio' 2>/dev/null; then pio_cmd=(python3 -m platformio)
  else echo 'Installer PlatformIO dans VS Code ou dans un environnement Python.' >&2; exit 1; fi
  case "$action" in
   build) exec "${pio_cmd[@]}" run ;;
   upload) exec "${pio_cmd[@]}" run --target upload ;;
   monitor) exec "${pio_cmd[@]}" device monitor --baud 115200 ;;
  esac ;;
 *) echo 'Usage : ./build.sh build|upload|monitor|test|direct' >&2; exit 2 ;;
esac
