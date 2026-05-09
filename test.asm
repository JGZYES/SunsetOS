bits 32

section .text
global _start

_start:
    mov edi, 0xB8000
    mov ecx, 80*25
    mov ax, 0x0F20
    rep stosw
    
    mov edi, 0xB8000
    mov esi, message
print_loop:
    lodsb
    test al, al
    jz done
    stosb
    mov al, 0x0F
    stosb
    jmp print_loop
done:
    hlt
    jmp done

section .data
message db 'Hello from ELF!'
        db 0