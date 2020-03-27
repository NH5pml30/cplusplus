LONG_QWORDS_LENGTH equ          128
                section         .text
                global          _start
_start:

                sub             rsp, 3 * LONG_QWORDS_LENGTH * 8
                lea             rdi, [rsp + LONG_QWORDS_LENGTH * 8]
                mov             rcx, LONG_QWORDS_LENGTH
                call            read_long
                mov             rdi, rsp
                call            read_long

                lea             rsi, [rsp + LONG_QWORDS_LENGTH * 8]
                lea             rdx, [rsp + 2 * LONG_QWORDS_LENGTH * 8]
                call            mul_long_long
                mov             rsi, product_msg
                mov             rdx, product_msg_size
                call            print_string
                lea             rdi, [rsp + 2 * LONG_QWORDS_LENGTH * 8]
                call            write_long
                call            write_new_line

                jmp             exit

; adds 64-bit number to long number
;    rdi -- address of summand #1 (long number)
;    rax -- summand #2 (64-bit unsigned)
;    rcx -- length of long number in qwords
; modifies:
;    rax
; result:
;    sum is written to rdi
add_long_short:
                push            rdi
                push            rcx
                push            rdx

                xor             rdx,rdx
.loop:
                add             [rdi], rax
                adc             rdx, 0
                mov             rax, rdx
                xor             rdx, rdx
                add             rdi, 8
                dec             rcx
                jnz             .loop

                pop             rdx
                pop             rcx
                pop             rdi
                ret

; multiplies long number by a short
;    rdi -- address of multiplier #1 (long number)
;    rbx -- multiplier #2 (64-bit unsigned)
;    rcx -- length of long number in qwords
; modifies:
;    rdx, rsi
; result:
;    product is written to rdi
mul_long_short:
                push            rax
                push            rdi
                push            rcx

                xor             rsi, rsi
.loop:
                mov             rax, [rdi]
                mul             rbx
                add             rax, rsi
                adc             rdx, 0
                mov             [rdi], rax
                add             rdi, 8
                mov             rsi, rdx
                dec             rcx
                jnz             .loop

                pop             rcx
                pop             rdi
                pop             rax
                ret

; multiplies long number by a qword-wise left-shifted 64-bit unsigned and adds result to another long
;    rdi -- address of multiplier #1 (long number)
;    rcx -- length of long number in qwords
;    rbx -- multiplier #2 (64-bit unsigned)
;    r8  -- number of qwords to left-shift multiplier #2
;    rdx -- result address (long number)
; modifies:
;    r9
; result:
;    product is added to long at rdi
add_long_mul_long_shifted_short:
                push            rax
                push            rdi
                push            rcx
                push            rdx
                push            rsi

                lea             r9, [rdx + 8 * r8]
                sub             rcx, r8

                xor             rsi, rsi
.loop:
                mov             rax, [rdi]
                mul             rbx
                add             rax, rsi
                adc             rdx, 0
                add             [r9], rax
                adc             rdx, 0
                add             rdi, 8
                add             r9, 8
                mov             rsi, rdx
                dec             rcx
                jnz             .loop

                pop             rsi
                pop             rdx
                pop             rcx
                pop             rdi
                pop             rax
                ret

; multiplies two long numbers
;    rdi -- address of multiplier #1 (long number)
;    rsi -- address of multiplier #2 (long number)
;    rdx -- address of result (long number)
;    rcx -- length of long numbers in qwords
; modifies:
;    rbx, r8, r9
; result:
;    product is written to rdx
mul_long_long:
                push            rax
                push            rsi
                push            rcx

                push            rdi
                mov             rdi, rdx
                call            set_zero
                pop             rdi
                mov             rax, rcx
                xor             r8, r8

.loop:
                mov             rbx, [rsi]
                call            add_long_mul_long_shifted_short
                inc             r8
                add             rsi, 8
                dec             rax
                jnz             .loop

                pop             rcx
                pop             rsi
                pop             rax
                ret


; divides long number by a short
;    rdi -- address of dividend (long number)
;    rbx -- divisor (64-bit unsigned)
;    rcx -- length of long number in qwords
; modifies:
;    rdx
; result:
;    quotient is written to rdi
;    rdx -- remainder
div_long_short:
                push            rdi
                push            rax
                push            rcx

                lea             rdi, [rdi + 8 * rcx - 8]
                xor             rdx, rdx

.loop:
                mov             rax, [rdi]
                div             rbx
                mov             [rdi], rax
                sub             rdi, 8
                dec             rcx
                jnz             .loop

                pop             rcx
                pop             rax
                pop             rdi
                ret

; assigns a zero to long number
;    rdi -- argument (long number)
;    rcx -- length of long number in qwords
; modifies:
;    none
set_zero:
                push            rax
                push            rdi
                push            rcx

                xor             rax, rax
                rep stosq

                pop             rcx
                pop             rdi
                pop             rax
                ret

; checks if a long number is a zero
;    rdi -- argument (long number)
;    rcx -- length of long number in qwords
; modifies:
;    none
; result:
;    ZF=1 if zero
is_zero:
                push            rax
                push            rdi
                push            rcx

                xor             rax, rax
                rep scasq

                pop             rcx
                pop             rdi
                pop             rax
                ret

; read long number from stdin
;    rdi -- location for output (long number)
;    rcx -- length of long number in qwords
; modifies:
;    rax, rbx, rsi (if no invalid characters are read)
read_long:
                push            rcx
                push            rdi

                call            set_zero
.loop:
                call            read_char
                or              rax, rax
                js              exit
                cmp             rax, 0x0a
                je              .done
                cmp             rax, '0'
                jb              .invalid_char
                cmp             rax, '9'
                ja              .invalid_char

                sub             rax, '0'
                mov             rbx, 10
                call            mul_long_short
                call            add_long_short
                jmp             .loop

.done:
                pop             rdi
                pop             rcx
                ret

.invalid_char:
                mov             rsi, invalid_char_msg
                mov             rdx, invalid_char_msg_size
                call            print_string
                call            write_char
                mov             al, 0x0a
                call            write_char

.skip_loop:
                call            read_char
                or              rax, rax
                js              exit
                cmp             rax, 0x0a
                je              exit
                jmp             .skip_loop

; write long number to stdout
;    rdi -- argument (long number)
;    rcx -- length of long number in qwords
; modifies:
;    rbx, rbp
; result:
;    at rdi -- 0
write_long:
                push            rax
                push            rcx
                push            rdx
                push            rsi

                mov             rax, 20
                mul             rcx
                mov             rbp, rsp
                sub             rsp, rax

                mov             rsi, rbp

.loop:
                mov             rbx, 10
                call            div_long_short
                add             rdx, '0'
                dec             rsi
                mov             [rsi], dl
                call            is_zero
                jnz             .loop

                mov             rdx, rbp
                sub             rdx, rsi
                call            print_string

                mov             rsp, rbp

                pop             rsi
                pop             rdx
                pop             rcx
                pop             rax
                ret

; write new line character to stdout
; modifies:
;    rax
write_new_line:
                push            rcx
                push            rdi
                push            rsi

                mov             al, 0x0a
                call            write_char

                pop             rsi
                pop             rdi
                pop             rcx
                ret

; read one char from stdin
; modifies:
;    rax, rsi
; result:
;    rax == -1 if error occurs
;    rax \in [0; 255] if OK
read_char:
                push            rcx
                push            rdi

                sub             rsp, 1
                xor             rax, rax
                xor             rdi, rdi
                mov             rsi, rsp
                mov             rdx, 1
                syscall

                cmp             rax, 1
                jne             .error
                xor             rax, rax
                mov             al, [rsp]
                add             rsp, 1

                pop             rdi
                pop             rcx
                ret
.error:
                mov             rax, -1
                add             rsp, 1
                pop             rdi
                pop             rcx
                ret

; write one char to stdout, errors are ignored
; modifies:
;    rax, rcx, rdi, rsi
; result:
;    al -- char
write_char:
                sub             rsp, 1
                mov             [rsp], al

                mov             rax, 1
                mov             rdi, 1
                mov             rsi, rsp
                mov             rdx, 1
                syscall
                add             rsp, 1
                ret

exit:
                mov             rax, 60
                xor             rdi, rdi
                syscall

; print string to stdout
;    rsi -- string
;    rdx -- size
; modifies:
;    none
print_string:
                push            rax
                push            rcx
                push            rdi

                mov             rax, 1
                mov             rdi, 1
                syscall

                pop             rdi
                pop             rcx
                pop             rax
                ret


                section         .rodata
invalid_char_msg:
                db              "Invalid character: "
invalid_char_msg_size: equ             $ - invalid_char_msg
product_msg:    db              "Product: "
product_msg_size: equ           $ - product_msg
