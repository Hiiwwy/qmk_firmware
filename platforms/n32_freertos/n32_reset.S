// Copyright 2026
// SPDX-License-Identifier: GPL-2.0-or-later

    .syntax unified
    .cpu cortex-m4
    .fpu fpv4-sp-d16
    .thumb

    .global Reset_Handler
    .type Reset_Handler, %function

Reset_Handler:
    cpsid i

    ldr r0, =_estack
    mov sp, r0

    ldr r0, =g_pfnVectors
    ldr r1, =0xE000ED08
    str r0, [r1]
    dsb
    isb

    ldr r0, =0xE000E010
    movs r1, #0
    str r1, [r0, #0]
    str r1, [r0, #4]
    str r1, [r0, #8]

    ldr r0, =0xE000E180
    ldr r1, =0xE000E280
    ldr r2, =0xFFFFFFFF
    movs r3, #8

1:
    str r2, [r0], #4
    str r2, [r1], #4
    subs r3, r3, #1
    bne 1b
    dsb
    isb

    ldr r0, =_sdata
    ldr r1, =_edata
    ldr r2, =_sidata
    movs r3, #0
    b 3f

2:
    ldr r4, [r2, r3]
    str r4, [r0, r3]
    adds r3, r3, #4

3:
    adds r4, r0, r3
    cmp r4, r1
    bcc 2b

    ldr r0, =_sbss
    ldr r1, =_ebss
    movs r2, #0
    b 5f

4:
    str r2, [r0]
    adds r0, r0, #4

5:
    cmp r0, r1
    bcc 4b

    bl SystemInit
    bl __libc_init_array
    bl main

6:
    b 6b

    .size Reset_Handler, .-Reset_Handler
