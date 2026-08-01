/* ---------------------------------------------------------------------------
 *  Linker script — ATSAMD51J19A on an Adafruit Metro M4 Express.
 *
 *  Memory sizes are from the SAM D5x/E5x data sheet, Table 9-1 "Physical
 *  Memory Map" (p.50).  The part is a ...51J19A, so the x19 column applies:
 *  512 KB of flash at 0x00000000 and 192 KB of SRAM at 0x20000000.
 *
 *  The application does NOT start at 0x00000000.  The first 16 KB of flash
 *  hold Adafruit's UF2 bootloader — the thing that gives you the METROM4BOOT
 *  drive on a double-tap of RESET — so the image is linked at 0x4000 and is
 *  16 KB shorter.  (Metro M4 learn guide p.32: "For M4 boards, which have a
 *  16kB bootloader, you must specify -offset=0x4000".)  Link it at 0 instead
 *  and flashing it destroys the bootloader; the board then needs an SWD probe
 *  to recover.
 * ------------------------------------------------------------------------- */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH (rx)  : ORIGIN = 0x00004000, LENGTH = 512K - 16K
  RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 192K
}

/* The stack grows DOWNWARD from the top of RAM, so the initial stack pointer
 * is one past the last byte.  Vector table entry 0 is loaded with this. */
_estack = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .text :
  {
    /* KEEP, because nothing references vector_table by name — it is found by
     * the hardware at a fixed address.  Without KEEP, --gc-sections would see
     * an unreferenced array and discard the entire vector table. */
    KEEP(*(.vectors))
    *(.text*)
    *(.rodata*)
    . = ALIGN(4);
  } > FLASH

  /* Unwind tables.  Nothing here throws or unwinds; discard them so they
   * cannot silently land between .text and .data and shift the load address. */
  /DISCARD/ :
  {
    *(.ARM.exidx*)
    *(.ARM.extab*)
  }

  /* .data is the awkward one: it is ADDRESSED in RAM but STORED in flash.
   * `> RAM AT > FLASH` expresses exactly that split, and _sidata records where
   * the flash copy ended up so Reset_Handler can find it. */
  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH

  _sidata = LOADADDR(.data);

  /* NOLOAD: .bss occupies RAM but contributes no bytes to the image.  Leave it
   * out and the .bin grows by the size of every zero-initialised global. */
  .bss (NOLOAD) :
  {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM

  _end = .;
}
