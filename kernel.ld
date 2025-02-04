ENTRY(boot) /* The entry point of the kernel is the boot function. */

SECTIONS {
    . = 0x80200000; /* The base address. '.' is the current address. */
    __kernel_base = .; /* The starting address used by the kernel. */
    /* Kernel's virtual addresses match the physical addresses (i.e. vaddr == paddr) */

    /* This section contains the code of the program. */
    .text :{
        KEEP(*(.text.boot));
        *(.text .text.*);
    }

    /* This section contains constant data that is read-only. */
    .rodata : ALIGN(4) {
        *(.rodata .rodata.*);
    }

    /* This section contains read/write data. */
    .data : ALIGN(4) {
        *(.data .data.*);
    }

    /* This section contains read/write data with an initial value of zero. */
    .bss : ALIGN(4) {
        __bss = .;
        *(.bss .bss.* .sbss .sbss.*);
        __bss_end = .;
    }

    . = ALIGN(4);
    . += 128 * 1024; /* "Advance the current address by 128KB */
    __stack_top = .;

    . = ALIGN(4096); /* 0x1000 */
    __free_ram = .;
    . += 64 * 1024 * 1024; /* 64 MB */
    __free_ram_end = .;
}
