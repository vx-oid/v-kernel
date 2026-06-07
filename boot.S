.global _start
.global trap_vector

/* Put the vectored trap jump table in its own section so the linker can
    place it at the start of the .text region. */
.section .text.trap_vector_table
.align 8
/* Vectored table: each entry jumps to the common `trap_vector` handler.
    Hardware in vectored mode will jump to mtvec_base + 4*cause; these
    stubs forward every possible cause to the unified handler. */
trap_vector_table:
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j timer_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector
    j trap_vector

trap_vector:
    # 1. Allocate 256 bytes on the stack to save registers and provide C-call safety
    addi sp, sp, -256

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

    # 3. Pass pointer to saved register frame in a0 and call the C Handler
    addi a0, sp, 0
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
    addi sp, sp, 256

    # 6. Machine Return (jump back to where we were before the interrupt)
    mret

/* Minimal vectored timer handler for cause 7 (machine-timer).
    This will run before the common trap handler and must clear the
    hardware timer interrupt to avoid re-entering. */
timer_vector:
     # Small stack frame to preserve ra
     addi sp, sp, -16
     sw ra, 0(sp)

     # Increment the systicks counter (systicks is a global C symbol)
     la t0, systicks
     lw t1, 0(t0)
     addi t1, t1, 1
     sw t1, 0(t0)

     # Call C helper to clear the timer interrupt source
    la t0, clear_timer_interrupt
    jalr ra, t0, 0

     lw ra, 0(sp)
     addi sp, sp, 16
     mret

_start:
    # Set the Stack Pointer (sp) to the top of the C3's internal SRAM
    # Use a 16-byte aligned stack top to satisfy RISC-V ABI and avoid
    # misaligned store exceptions when saving caller-saved registers.
    # Align 0x3FCDFFFF down to the nearest 16-byte boundary: 0x3FCDFFF0
    li sp, 0x3FCDFFF0

    # Zero the .bss section so C globals without initializers start at 0.
    la t0, __bss_start
    la t1, __bss_end
1:
    bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:

    # Set mtvec early to trap_vector_table in direct mode (clear low two bits)
    la t0, trap_vector_table
    li t1, -4
    and t0, t0, t1
    csrw mtvec, t0

    # Jump to our C code
    call main

    # If main ever crashes or returns, trap the CPU in an infinite loop
1:  j 1b