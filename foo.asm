  device zxspectrumnext

  org $2000

start:
  ld hl,message
loop: 
  ld a,(hl)
  or a
  ret z
  rst $10
  inc hl
  jp loop

message:
  db 'Hello, world!',$0d,0
