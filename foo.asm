  ;; To execute from BASIC:
  ;;
  ;; 10 CLEAR 32767
  ;; 20 LOAD "foo.bin" CODE 32768
  ;; 30 RANDOMIZE USR 32768
  ;; 40 PAUSE 0

  device zxspectrumnext

  org $8000

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
