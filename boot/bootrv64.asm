# bootrv64.asm
L_HLT:
    wfi
    ret
V_PageTableLoad:        ; a0 = root | (mode<<60) | (asid<<44)
    csrw satp, a0
    sfence.vma
    ret
B_TLBFlush:
    sfence.vma
    ret
