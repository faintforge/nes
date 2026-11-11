.segment "HEADER"
    .byte "NES", $1A
    .byte 2   ; 2 x 16KB PRG-ROM = 32KB (ld65 -t places the vectors at $8000 so our ROM has to be 32KB).
    .byte 0   ; 0 CHR-ROM (CHR-RAM)
    .byte $00 ; Mapper 0 - vertical arragement
    .byte $08 ; NES 2.0 identifier

.segment "VECTORS"
    .word nmi
    .word reset
    .word irq

.segment "CHARS"
.segment "STARTUP"

; Constants
PPUADDR = $2006
PPUDATA = $2007

.segment "CODE"

reset:
    ; https://www.nesdev.org/wiki/Init_code
    sei       ; Ignore IRQs
    cld       ; Disable decimal mode
    ldx #$40
    ldx $4017 ; Disable APU frame IRQ
    ldx #$FF
    txs       ; Set stack pointer to $01FF
    inx       ; now X = 0
    stx $2000 ; Disable NMI
    stx $2001 ; Disable rendering
    stx $4010 ; Disable DMC IRQs

    bit $2002
vblankwait1:
    bit $2002
    bpl vblankwait1

    txa
clrmem:
    sta $0000,x
    sta $0100,x
    sta $0200,x
    sta $0300,x
    sta $0400,x
    sta $0500,x
    sta $0600,x
    sta $0700,x
    inx
    bne clrmem

vblankwait2:
    bit $2002
    bpl vblankwait2

setup_palette:
    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR
    ; Bright red
    lda #$26
    sta PPUDATA
    ; Magenta
    lda #$25
    sta PPUDATA

    lda #$3F
    sta PPUADDR
    lda #$00
    sta PPUADDR

    lda #$80
    sta $2000 ; Enable NMI
    ldx #$00

main:
    jmp main

irq:
    rti

nmi:
    ; Every 10 frames, change color
    ; X is counter
    ; Y is PPUADDR lo
    ; Z not set
    cpx #20
    bne palette_skip

    ldx #$00

    stx $00
    iny
    tya
    and #$01
    tay

    lda #$3F
    sta PPUADDR
    sty PPUADDR
palette_skip:
    inx
    rti
