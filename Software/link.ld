ENTRY(Reset_Handler);
MEMORY {
  flash(rx)  : ORIGIN = 0x08000000, LENGTH = 32k
  sram(rwx) : ORIGIN = 0x20000000, LENGTH = 12k 
}
_estack     = ORIGIN(sram) + LENGTH(sram);    /* stack points to end of SRAM */

SECTIONS {
  .vectors  : { KEEP(*(.isr_vector)) } > flash
  .text     : { *(.text*) }            > flash
  .rodata : { *(.rodata*) . = ALIGN(4); } > flash

  .data : ALIGN(4) {
    _sdata = .;
    *(.first_data)
    *(.data SORT(.data.*))
    . = ALIGN(4);
    _edata = .;
  } > sram AT > flash
  _sidata = LOADADDR(.data);

  .bss : ALIGN(4) {
    _sbss = .;
    *(.bss SORT(.bss.*) COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > sram

  . = ALIGN(8);
  _end = .;     /* for cmsis_gcc.h  */
}