.org 0x1000
mov ax, 2000
int 4
mov ax, msg
int 2
mov ax, 2000
int 4
mov ax, 0x01
int 3
mov ax, hello
int 2

mov ax, 0x03
int 1


hlt
msg: db "Hello, this is my asembler!", 0
hello: db "@Loxsete my telegram :)", 0

