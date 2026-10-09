ENTRY(_start)
MEMORY {
  rom   : ORIGIN = 0x08000000, LENGTH = 32M
  iwram : ORIGIN = 0x03000000, LENGTH = 31K
  ewram : ORIGIN = 0x02000000, LENGTH = 256K
}
SECTIONS {
  .text : { KEEP(*(.crt0)) *(EXCLUDE_FILE(*libgcc.a:*) .text*) *(.rodata*) *(.glue_7) *(.glue_7t) . = ALIGN(4); } > rom
  .ARM.exidx : { *(.ARM.exidx*) } > rom
  .iwram : { __iwram_start = .; *(.iwram*) *libgcc.a:*(.text*) . = ALIGN(4); __iwram_end = .; } > iwram AT > rom
  __iwram_lma = LOADADDR(.iwram);
  .data : { __data_start = .; *(.data*) . = ALIGN(4); __data_end = .; } > iwram AT > rom
  __data_lma = LOADADDR(.data);
  .bss (NOLOAD) : { __bss_start = .; *(.bss*) *(COMMON) . = ALIGN(4); __bss_end = .; } > iwram
  .ewram (NOLOAD) : { __ewram_start = .; *(.ewram*) *(.sbss*) . = ALIGN(4); __ewram_end = .; } > ewram
  /DISCARD/ : { *(.ARM.attributes) *(.comment) }
}
