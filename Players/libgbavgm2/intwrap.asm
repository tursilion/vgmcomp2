/* basic interrupt wrapper */

	.extern	InterruptProcess
    .align
	.code 32
	.global	intrwrap

intrwrap:
                                         @ Single interrupts support
        mov     r3, #0x4000000           @ REG_BASE
        ldr     r2, [r3,#0x200]!         @ Read REG_IE
        and     r1, r2, r2, lsr #16      @ r1 = IE & IF

        strh    r0, [r3, #2]             @ IF Clear
        ldr     r0, =InterruptProcess
        bx      r0
