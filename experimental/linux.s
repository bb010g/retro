bits 32
section .text
global putchar
global getchar
global _start
extern main

align 4
_start:
	call main
	jmp $

align 4
putchar:
	mov eax, [esp+4]
	mov [buf], eax
	mov edx, 1
	mov ecx, buf
	mov ebx, 1
	mov eax, 4
	int 0x80
	ret

align 4
getchar:
	mov edx, 1
	mov ecx, buf
	mov ebx, 0
	mov eax, 3
	int 0x80
	mov eax, 0
	mov eax, [buf]
	ret

section .data
buf:
	dd 0
	dd 0
	dd 0
	dd 0
