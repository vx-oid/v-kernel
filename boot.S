.global _start

_start:
    # Set the Stack Pointer (sp) to the top of the C3's internal SRAM
    # DRAM starts at 0x3FC80000 and is 320KB long.
    li sp, 0x3FCDFFFF 
    
    # Jump to our C code
    call main

    # If main ever crashes or returns, trap the CPU in an infinite loop
1:  j 1b
