.org 0x1000

test_file: db "test.txt", 0
write_data: db "Hello, Disk!", 0
read_buffer: db 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
msg_disk_create: db "Disk create: OK", 0
msg_disk_write: db "Disk write: OK", 0
msg_disk_read: db "Disk read: ", 0
msg_error: db "Error occurred", 0
newline: db 10, 0

main:
    ; Test disk: create file
    mov ax, 0x04        ; Interrupt 10, function 0x04 (create file)
    mov bx, test_file   ; Filename address
    int 10              ; Trigger disk interrupt
    jz disk_create_ok   ; Jump if successful
    jmp disk_error

disk_create_ok:
    mov ax, msg_disk_create ; Load success message
    int 2               ; Print message
    mov ax, newline     ; Load newline
    int 2               ; Print newline

    ; Test disk: write data
    mov ax, 0x02        ; Interrupt 10, function 0x02 (write)
    mov bx, write_data  ; Data address
    mov cx, 13          ; Length of write_data (including null)
    int 10              ; Trigger disk interrupt
    jz disk_write_ok    ; Jump if successful
    jmp disk_error

disk_write_ok:
    mov ax, msg_disk_write ; Load success message
    int 2               ; Print message
    mov ax, newline     ; Load newline
    int 2               ; Print newline

    ; Test disk: read data
    mov ax, 0x01        ; Interrupt 10, function 0x01 (read)
    mov bx, read_buffer ; Buffer address
    mov cx, 13          ; Length to read
    int 10              ; Trigger disk interrupt
    jz disk_read_ok     ; Jump if successful
    jmp disk_error

disk_read_ok:
    mov ax, msg_disk_read ; Load read message
    int 2               ; Print message
    mov ax, read_buffer ; Load buffer address
    int 2               ; Print read data
    mov ax, newline     ; Load newline
    int 2               ; Print newline

    hlt                 ; Halt program

disk_error:
    mov ax, msg_error   ; Load error message
    int 2               ; Print error message
    mov ax, newline     ; Load newline
    int 2               ; Print newline
    hlt                 ; Halt program
