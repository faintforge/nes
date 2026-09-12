.segment "VECTORS"
.word nmi
.word reset
.word irq

.segment "CODE"
reset:
    ldx $FD
    txs
    lda #42
    brk

nmi:
    ldx #1
    stx $FF00
    rti

irq:
    ldx #0
    stx $FF00
    rti
