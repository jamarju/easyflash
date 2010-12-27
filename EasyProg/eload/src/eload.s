
.include "kernal.i"

.importzp       sp, sreg, regsave
.importzp       ptr1, ptr2, ptr3, ptr4

.import eload_set_read_byte_fn
.import eload_read_byte_from_buffer
.import eload_read_byte_kernal
.import eload_read_byte_fast
.import eload_buffered_byte



.import loader_upload_code


; loader_send and eload_recv are for communicating with the drive
.export loader_send

.rodata

        .align 16
sendtab:
        .byte $00, $80, $20, $a0
        .byte $40, $c0, $60, $e0
        .byte $10, $90, $30, $b0
        .byte $50, $d0, $70, $f0
sendtab_end:
        .assert >sendtab_end = >sendtab, error, "sendtab mustn't cross page boundary"

.bss

; 0 if everything is okay, otherwise an error happened or EOF was reached
eload_status:
        .res 1

; remaining number of bytes in this sector
.export eload_ctr
eload_ctr:
        .res 1

.code

; =============================================================================
;
; Open the file for read access. Check _eload_status for the result.
;
; int __fastcall__ eload_open_read(const char* name);
;
; parameters:
;       pointer to name in AX (A = low)
;
; return:
;       result in AX (A = low), 0 = okay, -1 = error
;
; =============================================================================
        .export _eload_open_read
_eload_open_read:
        sta ptr1
        stx ptr1 + 1
        lda #0
        sta ST                  ; set status to OK
        lda $ba                 ; set drive to listen
        jsr LISTEN
        lda #$f0                ; open + secondary addr 0
        jsr SECOND

        ldy #0
@send_name:
        lda (ptr1),y            ; send file name
        beq @end_name           ; 0-termination
        jsr CIOUT
        iny
        bne @send_name          ; branch always (usually)
@end_name:
        jsr UNLSN

        ; give up if we couldn't even send the file name
        lda ST
        bne @fail

        ; Check if the file is readable
        lda $ba
        jsr TALK
        lda #$60                ; talk + secondary addr 0
        jsr TKSA
        jsr ACPTR               ; read a byte
        sta eload_buffered_byte       ; keep it for later
        jsr UNTLK

        lda ST
        bne @close_and_fail

        jsr loader_upload_code
        bcs @use_kernal

        lda #<eload_read_byte_fast
        ldx #>eload_read_byte_fast
        jsr eload_set_read_byte_fn

        lda #1                  ; command: load
        jsr loader_send
        jsr eload_recv         ; status / number of bytes

        sta eload_ctr
        cmp #$ff
        beq @close_and_fail
        bne @ok

@use_kernal:
        ; no suitable speeder found, use Kernal
        lda #<eload_read_byte_from_buffer
        ldx #>eload_read_byte_from_buffer
        jsr eload_set_read_byte_fn

        ; send TALK so we can read the bytes afterwards
        lda $ba
        jsr TALK
        lda #$60
        jsr TKSA
@ok:
        lda #0
        tax
        sta eload_status
        rts

@close_and_fail:
        lda $ba                 ; set drive to listen
        jsr LISTEN
        lda #$e0                ; close + channel 0
        jsr SECOND
        jsr UNLSN
@fail:
        lda #$ff
        tax
        sta eload_status
        rts


; send a byte to the drive
loader_send:
    sta loader_send_savea
loader_send_do:
    sty loader_send_savey

    pha
    lsr
    lsr
    lsr
    lsr
    tay

    lda $dd00
    and #7
    sta $dd00
    sta savedd00
    eor #$07
    ora #$38
    sta $dd02

@waitdrv:
    bit $dd00       ; wait for drive to signal ready to receive
    bvs @waitdrv        ; with CLK low

    lda $dd00       ; pull DATA low to acknowledge
    ora #$20
    sta $dd00

@wait2:
    bit $dd00       ; wait for drive to release CLK
    bvc @wait2

    sei

loader_send_waitbadline:
    lda $d011       ; wait until a badline won't screw up
    clc         ; the timing
    sbc $d012
    and #7
    beq loader_send_waitbadline
loader_send_nobadline:

    lda $dd00       ; release DATA to signal that data is coming
    ;ora #$20
    and #$df
    sta $dd00

    lda sendtab,y       ; send the first two bits
    sta $dd00

    lsr
    lsr
    and #%00110000      ; send the next two
    sta $dd00

    pla         ; get the next nybble
    and #$0f
    tay
    lda sendtab,y
    sta $dd00

    lsr         ; send the last two bits
    lsr
    and #%00110000
    sta $dd00

    nop         ; slight delay, and...
    nop
    lda savedd00        ; restore $dd00 and $dd02
    sta $dd00
    lda #$3f
    sta $dd02

    ldy loader_send_savey
    lda loader_send_savea

    cli
    rts

savedd00:       .res 1
loader_send_savea:  .res 1
loader_send_savey:  .res 1


; =============================================================================
;
; Receive a byte from the drive over the fast protocol. Used internally only.
;
; parameters:
;       -
;
; return:
;       Byte in A, Z-flag according to A
;
; changes:
;       flags
;
; =============================================================================
.export eload_recv
eload_recv:
        ; $dd00: | D_in | C_in | D_out | C_out || A_out | RS232 | VIC | VIC |
        ; Note about the timing: After 50 cycles a PAL C64 is about 1 cycle
        ; slower than the drive, an NTSC C64 is about 1 cycle faster. As we
        ; have a safety gap of about 2 us, this doesn't matter.

        ; Handshake Step 1: Drive signals byte ready with DATA low
@wait1:
        lda $dd00
        bmi @wait1

        sei

@eload_recv_waitbadline:
        lda $d011               ; wait until a badline won't screw up
        clc                     ; the timing
        sbc $d012
        and #7
        beq @eload_recv_waitbadline

        ; Handshake Step 2: Host sets CLK low to acknowledge
        lda $dd00
        ora #$10
        sta $dd00               ; [1]

        ; Handshake Step 3: Host releases CLK - Time base
        bit $ff                 ; waste 3 cycles
        and #$03
        ; an 1 MHz drive sees this 6..12 us after [1], so we have dt = 9
        sta $dd00               ; t = 0
        sta @eor+1              ; 4

        nop
        nop
        nop
        nop
        nop                     ; 14

        ; receive bits
        lda $dd00               ; 18 - b0 b1
        lsr
        lsr
        eor $dd00               ; 26 - b2 b3
        lsr
        lsr
        eor $dd00               ; 34 - b4 b5
        lsr
        lsr
@eor:
        eor #$00
        eor $dd00               ; 44 - b6 b7
        cli
        rts

