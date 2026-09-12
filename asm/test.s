.segment "VECTORS"
.word nmi
.word start
.word irq

.segment "CODE"
start:
    stx $4000
    ; set up stack
    ldx #$FF
    txs

    stx $4001

    lda #0
    jsr stop

reset:
    stx $4000
    ; set up stack
    ldx #$FF
    txs
    rts

stop:
    sta $7FFF
    brk

nmi:
    lda #2
    jmp stop

irq:
    lda #1
    jmp stop
