/* STM32L476RG : 1 Mo Flash ; 4 Ko réservés pour deux pages de configuration.
 * SRAM1 96 Ko utilisée ; SRAM2 32 Ko à 0x10000000 non allouée implicitement. */
ENTRY(Reset_Handler)
_estack = ORIGIN(RAM) + LENGTH(RAM);
_Min_Heap_Size = 0x1000;
_Min_Stack_Size = 0x2000;
MEMORY { RAM (xrw) : ORIGIN = 0x20000000, LENGTH = 96K
         FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1020K }
PHDRS { flash PT_LOAD FLAGS(5); ram PT_LOAD FLAGS(6); }
SECTIONS {
 .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } >FLASH :flash
 .text : { . = ALIGN(4); *(.text*) *(.glue_7*) *(.eh_frame) KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .; } >FLASH :flash
 .rodata : { . = ALIGN(4); *(.rodata*) . = ALIGN(4); } >FLASH :flash
 .ARM.extab : { *(.ARM.extab*) } >FLASH :flash
 .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } >FLASH :flash
 .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } >FLASH :flash
 .init_array : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } >FLASH :flash
 .fini_array : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } >FLASH :flash
 _sidata = LOADADDR(.data);
 .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } >RAM AT>FLASH :ram
 /* BSS initialisée à zéro par Reset_Handler ; ni pile ni tas ne sont chargés. */
 .bss (NOLOAD) : { . = ALIGN(4); _sbss = .; __bss_start__ = .; *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __bss_end__ = .; } >RAM :NONE
 ._user_heap_stack (NOLOAD) : { . = ALIGN(8); PROVIDE(end = .); PROVIDE(_end = .); _heap_start = .; . += _Min_Heap_Size; _heap_end = .; . += _Min_Stack_Size; . = ALIGN(8); } >RAM :NONE
 .ARM.attributes 0 : { *(.ARM.attributes) }
}
