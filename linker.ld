ENTRY(_start)

MEMORY {
    /* The C3's internal Instruction RAM */
    iram (rx) : ORIGIN = 0x40380000, LENGTH = 320K
    /* The C3's internal Data RAM */
    dram (rw) : ORIGIN = 0x3FC80000, LENGTH = 320K
}

SECTIONS {
    .text : {
        *(.text.trap_vector_table)
        *(.text*)
    } > iram

    .data : {
        *(.data*)
    } > dram

    .bss : {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        __bss_end = .;
    } > dram
}
