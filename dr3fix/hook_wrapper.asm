extern addr_stack_cookie:QWORD

extern fov:DWORD

extern addr_ExecuteWindowMessage:QWORD
hook_ExecuteWindowMessage proto

extern addr_HandlePlayerInput:QWORD
hook_HandlePlayerInput proto

extern addr_ProcessMouseInput:QWORD
hook_ProcessMouseInput proto

extern addr_UpdateCameraFreeRoam:QWORD
hook_UpdateCameraFreeRoam proto

extern addr_RestoreD3DDevice:QWORD
hook_RestoreD3DDevice_pre proto
hook_RestoreD3DDevice_post proto

.code

hook_ExecuteWindowMessage_wrap proc
        push    rcx
        push    rdx
        push    r8
        push    r9

        sub     rsp, 28h
        call    hook_ExecuteWindowMessage
        add     rsp, 28h

        pop     r9
        pop     r8
        pop     rdx
        pop     rcx

        test    al, al
        jz      go_to_orig

        ret

go_to_orig:
        ; Overwritten instructions
        mov	rax, rsp
        push    rsi
        push    rdi
        push    r12
        push    r14
        push    r15
        sub     rsp, 70h

        mov     rsi, [addr_ExecuteWindowMessage]
        add     rsi, 0Fh
        jmp     rsi
hook_ExecuteWindowMessage_wrap endp

hook_HandlePlayerInput_wrap proc
        push    rcx

        sub     rsp, 20h
        call    hook_HandlePlayerInput
        add     rsp, 20h

        pop     rcx

        ; Overwritten instructions
        mov     r11, rsp
        push    rbx
        sub     rsp, 0A0h

        mov     rax, [addr_stack_cookie]
        mov     rax, [rax]

        mov     rsi, [addr_HandlePlayerInput]
        add     rsi, 12h
        jmp     rsi
hook_HandlePlayerInput_wrap endp

hook_ProcessMouseInput_wrap proc
        push    rcx
        push    rdx
        push    r8
        push    r9

        lea     rdx, [rsp+10h]
        lea     r8, [rsp+8]
        lea     r9, [rsp]
        sub     rsp, 28h
        call    hook_ProcessMouseInput
        add     rsp, 28h

        pop     r9
        pop     r8
        pop     rdx
        pop     rcx

        ; Overwritten instructions
        mov     r11, rsp
        mov     [r11 + 20h], r9d
        mov     [r11 + 18h], r8d
        mov     [r11 + 10h], edx

        mov     rax, [addr_ProcessMouseInput]
        add     rax, 0Fh
        jmp     rax
hook_ProcessMouseInput_wrap endp

hook_UpdateCameraFreeRoam proc
        ; Set FOV
        mov     eax, [fov]
        mov     [rcx + 0DCh], eax

        ; Overwritten instructions
        mov     rax, rsp
        mov     [rax + 20h], rbx
        push    rdi
        sub     rsp, 0B0h

        mov     rdi, [addr_UpdateCameraFreeRoam]
        add     rdi, 0Fh
        jmp     rdi
hook_UpdateCameraFreeRoam endp

hook_RestoreD3DDevice_wrap proc
        push    rcx
        push    rdx

        sub     rsp, 28h
        call    hook_RestoreD3DDevice_pre
        add     rsp, 28h

        pop     rdx
        pop     rcx

        sub     rsp, 28h
        call    orig_RestoreD3DDevice
        call    hook_RestoreD3DDevice_post
        add     rsp, 28h
        ret
hook_RestoreD3DDevice_wrap endp

orig_RestoreD3DDevice proc
        ; Overwritten instructions
        push    rbp
        push    rsi
        push    rdi
        push    r12
        push    r13
        push    r14
        push    r15

        mov     rax, [addr_RestoreD3DDevice]
        add     rax, 0Ch
        jmp     rax
orig_RestoreD3DDevice endp

end