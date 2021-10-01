  ;; To execute from BASIC:
  ;;
  ;; 10 CLEAR %$5FFF
  ;; 20 LOAD "foo.bin" CODE %$6000
  ;; 30 RANDOMIZE % USR $6000
  ;; 40 PAUSE 0

  device zxspectrumnext

  org $6000

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
  db '??? Hello, world!',$0d,0
