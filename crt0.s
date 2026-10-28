    .section .crt0,"ax"
    .global _start
    .arm
_start:
    b rom_start
    .fill 188,1,0
rom_start:
    mov r0,#0x12
    msr cpsr_c,r0
    ldr sp,=0x03007fa0
    mov r0,#0x1f
    msr cpsr_c,r0
    ldr sp,=0x03007e80
    ldr r0,=__iwram_lma
    ldr r1,=__iwram_start
    ldr r2,=__iwram_end
1:  cmp r1,r2
    ldrlo r3,[r0],#4
    strlo r3,[r1],#4
    blo 1b
    ldr r0,=__data_lma
    ldr r1,=__data_start
    ldr r2,=__data_end
2:  cmp r1,r2
    ldrlo r3,[r0],#4
    strlo r3,[r1],#4
    blo 2b
    mov r3,#0
    ldr r1,=__bss_start
    ldr r2,=__bss_end
3:  cmp r1,r2
    strlo r3,[r1],#4
    blo 3b
    ldr r1,=__ewram_start
    ldr r2,=__ewram_end
4:  cmp r1,r2
    strlo r3,[r1],#4
    blo 4b
    ldr r0,=main
    mov lr,pc
    bx r0
    b .
    .pool
