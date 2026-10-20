#!/usr/bin/env bash
# Rôle : Compile et exécute les trois suites de tests C sur le PC.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
mkdir -p .test-build
cc -std=c11 -Wall -Wextra -Wno-misleading-indentation -fsanitize=address,undefined \
 -g -DHOST_TEST -Iinclude -Ilib/cesi_grove/include tests/test_core.c src/config.c src/time_service.c \
 src/gps.c lib/cesi_grove/src/grove_gps_air530z.c src/format.c -lm -o .test-build/test_core
.test-build/test_core
cc -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation \
 -fsanitize=address,undefined -g -DFF_USE_MKFS=1 -Itests/mock -Iinclude -Ilib/fatfs -Ilib/cesi_grove/include \
 tests/test_storage.c src/storage.c lib/cesi_grove/src/sd_logger_file.c src/format.c lib/fatfs/ff.c lib/fatfs/ffunicode.c \
 -lm -o .test-build/test_storage
.test-build/test_storage
cc -std=c11 -Wall -Wextra -Wno-unused-parameter -Wno-misleading-indentation \
 -fsanitize=address,undefined -g -DHOST_TEST -Itests/app_mock -Iinclude -Ilib/cesi_grove/include \
 tests/test_app.c src/app.c src/config.c src/time_service.c src/format.c \
 -lm -o .test-build/test_app
.test-build/test_app
