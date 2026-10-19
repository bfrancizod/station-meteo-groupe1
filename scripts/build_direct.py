#!/usr/bin/env python3
"""Compilation ARM locale sans lancer PlatformIO ni service distant.
SDK_STM32L4 : dossier STM32CubeL4 (Drivers/...), ARM_GCC : chemin gcc facultatif.
Les dépendances sont découvertes dans ~/.platformio/packages si disponibles.
"""
from pathlib import Path
import os, subprocess, shutil
from concurrent.futures import ThreadPoolExecutor
root=Path(__file__).resolve().parents[1]
packages=Path.home()/'.platformio/packages'
sdk=Path(os.environ.get('SDK_STM32L4',str(packages/'framework-stm32cubel4')))
gcc=os.environ.get('ARM_GCC',shutil.which('arm-none-eabi-gcc') or str(packages/'toolchain-gccarmnoneeabi/bin/arm-none-eabi-gcc'))
build=root/'build';build.mkdir(exist_ok=True)
hal=sdk/'Drivers/STM32L4xx_HAL_Driver'
# Isoler l'en-tête HAL principal pour charger la configuration du projet,
# même si le SDK partagé contient déjà une configuration générée par un autre projet.
shadow=build/'hal_include';shadow.mkdir(exist_ok=True)
shutil.copyfile(hal/'Inc/stm32l4xx_hal.h',shadow/'stm32l4xx_hal.h')
cmsis=sdk/'Drivers/CMSIS'
templates=cmsis/'Device/ST/STM32L4xx/Source/Templates'
flags=['-mcpu=cortex-m4','-mthumb','-mfpu=fpv4-sp-d16','-mfloat-abi=hard','-Os','-g3','-std=gnu11','-ffunction-sections','-fdata-sections','-fstack-usage','-Wall','-Wextra','-Wno-unused-parameter','-DSTM32L476xx','-DUSE_HAL_DRIVER','-DBME280_DOUBLE_ENABLE']
for directory in [root/'include',root/'lib/cesi_grove/include',shadow,root/'lib/fatfs',root/'lib/bosch_bme280',hal/'Inc',cmsis/'Include',cmsis/'Device/ST/STM32L4xx/Include']:flags+=['-I'+str(directory)]
sources=list((root/'src').glob('*.c'))+list((root/'lib/cesi_grove/src').glob('*.c'))+[root/'lib/fatfs/ff.c',root/'lib/fatfs/ffunicode.c',root/'lib/bosch_bme280/bme280.c',templates/'system_stm32l4xx.c']
for name in ['','_adc','_adc_ex','_cortex','_dma','_dma_ex','_flash','_flash_ex','_flash_ramfunc','_gpio','_i2c','_i2c_ex','_pwr','_pwr_ex','_rcc','_rcc_ex','_spi','_uart','_uart_ex']:sources.append(hal/'Src'/('stm32l4xx_hal'+name+'.c'))
startup=templates/'gcc/startup_stm32l476xx.s'
if not startup.exists():startup=startup.with_suffix('.S')
sources.append(startup)
def compile_one(item):
 index,source=item
 obj=build/(str(index)+'_'+source.stem+'.o')
 command=[gcc]+flags+(['-x','assembler-with-cpp'] if source.suffix.lower()=='.s' else [])+['-c',str(source),'-o',str(obj)]
 subprocess.run(command,check=True)
 return obj
with ThreadPoolExecutor(max_workers=4) as pool:objects=list(pool.map(compile_one,enumerate(sources)))
elf=build/'firmware.elf'
subprocess.run([gcc]+flags+[str(p) for p in objects]+['-T'+str(root/'linker.ld'),'-Wl,--gc-sections','-Wl,-Map='+str(build/'firmware.map'),'-Wl,-u,_printf_float','--specs=nano.specs','--specs=nosys.specs','-lm','-lc','-o',str(elf)],check=True)
prefix=gcc.removesuffix('gcc')
subprocess.run([prefix+'objcopy','-O','binary',str(elf),str(build/'firmware.bin')],check=True)
subprocess.run([prefix+'size',str(elf)],check=True)
print('Compilation ARM terminee :',elf)
