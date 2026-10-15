; Original 47FD90 uses 64-bit x87 significands before the final double store.
.code
PUBLIC dfre_palette_variance_x87
dfre_palette_variance_x87 PROC FRAME
    sub rsp, 24
    .allocstack 24
    .endprolog
    fnstcw WORD PTR [rsp+8]
    mov ax, WORD PTR [rsp+8]
    and ax, 0f0ffh
    or ax, 0300h
    mov WORD PTR [rsp+10], ax
    fldcw WORD PTR [rsp+10]
    fld QWORD PTR [rcx+16]
    fmul QWORD PTR [rcx+16]
    fld QWORD PTR [rcx+24]
    fmul QWORD PTR [rcx+24]
    faddp st(1), st(0)
    fld QWORD PTR [rcx+32]
    fmul QWORD PTR [rcx+32]
    faddp st(1), st(0)
    fdiv QWORD PTR [rcx]
    fsubr QWORD PTR [rcx+8]
    fstp QWORD PTR [rsp]
    fldcw WORD PTR [rsp+8]
    movsd xmm0, QWORD PTR [rsp]
    add rsp, 24
    ret
dfre_palette_variance_x87 ENDP
.const
palette_frequency_scale REAL8 65536.0
.code
PUBLIC dfre_palette_frequency_x87
dfre_palette_frequency_x87 PROC FRAME
    sub rsp, 40
    .allocstack 40
    .endprolog
    movsd QWORD PTR [rsp], xmm0
    movsd QWORD PTR [rsp+8], xmm1
    fnstcw WORD PTR [rsp+16]
    mov ax, WORD PTR [rsp+16]
    and ax, 0f0ffh
    or ax, 0f00h
    mov WORD PTR [rsp+18], ax
    fldcw WORD PTR [rsp+18]
    fld QWORD PTR [rsp]
    fmul QWORD PTR [rsp+8]
    fmul QWORD PTR [palette_frequency_scale]
    fistp DWORD PTR [rsp+24]
    fldcw WORD PTR [rsp+16]
    mov eax, DWORD PTR [rsp+24]
    add rsp, 40
    ret
dfre_palette_frequency_x87 ENDP
END
