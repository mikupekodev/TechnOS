; bootx64.asm
L_HLT:
    hlt
    ret
B_PageTableLoad:        ; rdi = root table (physical)
    mov cr3, rdi
    ret
B_TLBFlush:
    mov rax, cr3
    mov cr3, rax          ; reload で全体フラッシュ
    ret
