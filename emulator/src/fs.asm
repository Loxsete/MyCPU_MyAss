.org 0x1000

prompt: db "Input text ", 0
output_msg: db "Read from disk: ", 0
error_msg: db "Error!", 0
buffer: db "test", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  ; Инициализируем тестовыми данными
disk_addr: dw 0x100  ; Адрес на диске

main:
    ; Вывод приглашения
    mov ax, prompt
    int 2           ; BIOS interrupt 2: вывод строки

    ; Запись на диск
    mov ax, 0x02    ; BIOS функция 0x02: запись на диск
    mov bx, disk_addr
    mov cx, 16      ; Записать 16 байт
    int 10          ; Записать buffer на диск

    ; Проверка статуса диска
    mov ax, 0x03    ; BIOS функция 0x03: статус диска
    int 10
    cmp ax, 0
    jnz error       ; Если статус != 0, перейти к ошибке

    ; Очистка буфера вывода
    mov ax, 0x02    ; BIOS функция 0x02: очистить вывод
    int 3

    ; Чтение с диска
    mov ax, 0x01    ; BIOS функция 0x01: чтение с диска
    mov bx, disk_addr
    mov cx, 16      ; Прочитать 16 байт
    int 10          ; Прочитать в buffer

    ; Проверка статуса диска
    mov ax, 0x03
    int 10
    cmp ax, 0
    jnz error

    ; Вывод сообщения
    mov ax, output_msg
    int 2           ; Вывод "Прочитано с диска: "

    ; Вывод данных с диска
    mov ax, buffer
    int 2           ; Вывод содержимого buffer

    ; Добавление новой строки
    mov ax, 0x01    ; BIOS функция 0x01: добавить новую строку
    int 3

    hlt             ; Остановка

error:
    mov ax, error_msg
    int 2           ; Вывод сообщения об ошибке
    hlt
