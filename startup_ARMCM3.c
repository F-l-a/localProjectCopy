#include <stdint.h>

/* LINKER SCRIPT SYMBOLS */
extern uint32_t _etext;
extern uint32_t _sidata; /* Source address of the .data section in Flash (LMA) */
extern uint32_t _data;  /* Start of .data section in RAM (VMA) */
extern uint32_t _edata; /* End of .data section in RAM */
extern uint32_t _bss;   /* Start of .bss section in RAM */
extern uint32_t _ebss;  /* End of .bss section in RAM */
extern uint32_t _stack_top;

extern int main(void);

/* FreeRTOS exception handlers required for context switching and system ticks */
extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void); // Ora la usiamo come da FreeRTOSConfig.h modificato

/* SYSTEM RESET HANDLER
 This is the very first function executed upon power-up or reset.
 It prepares the C runtime environment before jumping to main(). */
void Reset_Handler(void) {
    uint32_t *src, *dest;

    /* 1. INITIALIZE .DATA SECTION */
    /* We use _sidata as the source address, NOT _etext.
     This ensures correct data initialization even if read-only data (.rodata) 
     or alignment padding is placed between the text and data sections in Flash. */
    src = &_sidata; 
    
    for (dest = &_data; dest < &_edata; ) {
        *dest++ = *src++;
    }

    /* 2. INITIALIZE .BSS SECTION 
     Zero out the BSS memory area (uninitialized global/static variables). */
    for (dest = &_bss; dest < &_ebss; ) {
        *dest++ = 0;
    }

    main();
    while (1);
}

/* DEFAULT FAULT HANDLER
Catch-all handler for unexpected interrupts and hardware faults (e.g. HardFault). */
void Default_Handler(void) {
    while (1);
}

/* VECTOR TABLE
 The vector table is placed at the beginning of the flash memory (address 0x00000000).
 It contains the initial stack pointer and the addresses of all exception and interrupt handlers. */
 
__attribute__((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
    (void (*)(void))(&_stack_top),
    Reset_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    Default_Handler,
    0, 0, 0, 0,
    vPortSVCHandler,
    Default_Handler,
    0,
    xPortPendSVHandler,
    xPortSysTickHandler,
};