.global _start
.global trap_vector

.align 4
trap_vector:
    # 1. Allocate 64 bytes on the stack to save the 16 "Caller-Saved" registers
    addi sp, sp, -64

    # 2. Save the registers to RAM
    sw ra, 0(sp)
    sw t0, 4(sp)
    sw t1, 8(sp)
    sw t2, 12(sp)
    sw a0, 16(sp)
    sw a1, 20(sp)
    sw a2, 24(sp)
    sw a3, 28(sp)
    sw a4, 32(sp)
    sw a5, 36(sp)
    sw a6, 40(sp)
    sw a7, 44(sp)
    sw t3, 48(sp)
    sw t4, 52(sp)
    sw t5, 56(sp)
    sw t6, 60(sp)

    # 3. Call the C Handler
    call c_trap_handler

    # 4. Restore the exact state of the CPU
    lw ra, 0(sp)
    lw t0, 4(sp)
    lw t1, 8(sp)
    lw t2, 12(sp)
    lw a0, 16(sp)
    lw a1, 20(sp)
    lw a2, 24(sp)
    lw a3, 28(sp)
    lw a4, 32(sp)
    lw a5, 36(sp)
    lw a6, 40(sp)
    lw a7, 44(sp)
    lw t3, 48(sp)
    lw t4, 52(sp)
    lw t5, 56(sp)
    lw t6, 60(sp)

    # 5. Deallocate the stack space
    addi sp, sp, 64

    # 6. Machine Return (jump back to where we were before the interrupt)
    mret

_start:
    # Set the Stack Pointer (sp) to the top of the C3's internal SRAM
    li sp, 0x3FCDFFFF

    # Jump to our C code
    call main

    # If main ever crashes or returns, trap the CPU in an infinite loop
1:  j 1b