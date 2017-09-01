/**
 * Link Script for Cabrio SRAM Booting.
 *
 * This link script assumes that the boot mode is configured to SARM mode, 
 * whcih means the booting is from SRAM. The image shall be written to 
 * SRAM before enable CPU.
 */

OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)



MEMORY
{
    SRAM(RWX): ORIGIN = 0x00000000, LENGTH = 192K
}


SECTIONS
{
    _STARTUP_LMA_START = ORIGIN(SRAM);
    _STARTUP_VMA_START = ORIGIN(SRAM);

    . = _STARTUP_LMA_START;

    .STARTUP _STARTUP_VMA_START : AT (_STARTUP_LMA_START)
    {
	*(.startup)

    } > SRAM



    _TEXT_LMA_START = _STARTUP_LMA_START + SIZEOF(.STARTUP);
    _TEXT_VMA_START = _STARTUP_VMA_START + SIZEOF(.STARTUP);
    
    .TEXT _TEXT_VMA_START : AT (_TEXT_LMA_START)
    {
	*(.text)
	*(.rodata)
	*(.rodata*)

    } > SRAM

    

    _DATA_LMA_START = _TEXT_LMA_START + SIZEOF(.TEXT);
    _DATA_VMA_START = _TEXT_VMA_START + SIZEOF(.TEXT);

    .DATA _DATA_VMA_START : AT (_DATA_LMA_START)
    {
	*(.data)

    }



    _BSS_LMA_START = _DATA_LMA_START + SIZEOF(.DATA);
    _BSS_VMA_START = _DATA_VMA_START + SIZEOF(.DATA);

    .BSS _BSS_VMA_START : AT (_BSS_LMA_START)
    {
	*(.bss)

    } > SRAM

    

}


