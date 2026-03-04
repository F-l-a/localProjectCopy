ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS
{
    .text :
    {
        . = ALIGN(4);
        _text = .;
        KEEP(*(.isr_vector))
        *(.text*)
        *(.rodata*)
        KEEP(*(.init))
        KEEP(*(.fini))
        *(.glue_7) 
        *(.glue_7t)
        *(.eh_frame)
        . = ALIGN(4);
        _etext = .;
    } > FLASH

    .ARM.exidx :
    {
        __exidx_start = .;
        *(.ARM.exidx*)
        __exidx_end = .;
    } > FLASH

    /* .data viene eseguita in RAM ma caricata in FLASH (AT > FLASH) */
    .data : 
    {
        . = ALIGN(4);
        _data = .;
        __data_start__ = .;
        *(.data*)
        . = ALIGN(4);
        _edata = .;
        __data_end__ = .;
    } > RAM AT > FLASH

    /* Simbolo magico per dire allo startup dove copiare i dati da */
    _sidata = LOADADDR(.data);

    .bss :
    {
        . = ALIGN(4);
        _bss = .;
        __bss_start__ = .;
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
        __bss_end__ = .;
        __end__ = .;
        end = .;
    } > RAM

    .stack (NOLOAD):
    {
        . = ALIGN(8);
        _stack_bottom = .;
        . = . + 0x2000;
        _stack_top = .;
    } > RAM
}