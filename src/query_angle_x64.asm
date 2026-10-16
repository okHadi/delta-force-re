.const
angle_scale REAL8 0.0000152587890625
angle_pi REAL8 3.1415926539
angle_half REAL8 0.5
angle_turn REAL8 4294967295.0
.code
PUBLIC dfre_actor_query_angle_x87
dfre_actor_query_angle_x87 PROC FRAME
    sub rsp,40
    .allocstack 40
    .endprolog
    mov DWORD PTR [rsp],ecx
    mov DWORD PTR [rsp+4],edx
    fnstcw WORD PTR [rsp+8]
    mov ax,WORD PTR [rsp+8]
    and ax,0f0ffh
    or ax,0300h
    mov WORD PTR [rsp+10],ax
    or ax,0c00h
    mov WORD PTR [rsp+12],ax
    fldcw WORD PTR [rsp+10]
    fild DWORD PTR [rsp]
    fmul QWORD PTR [angle_scale]
    fild DWORD PTR [rsp+4]
    fmul QWORD PTR [angle_scale]
    fdivrp st(1),st(0)
    fld1
    fpatan
    fdiv QWORD PTR [angle_pi]
    fmul QWORD PTR [angle_half]
    fmul QWORD PTR [angle_turn]
    fldcw WORD PTR [rsp+12]
    frndint
    fldcw WORD PTR [rsp+10]
    fistp QWORD PTR [rsp+16]
    fldcw WORD PTR [rsp+8]
    mov rax,QWORD PTR [rsp+16]
    add rsp,40
    ret
dfre_actor_query_angle_x87 ENDP
END
