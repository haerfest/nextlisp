  ;; To execute from BASIC:
  ;;
  ;; 10 CLEAR %$5FFF
  ;; 20 LOAD "foo.bin" CODE %$6000
  ;; 30 RANDOMIZE % USR $6000
  ;; 40 PAUSE 0

  device zxspectrumnext

  ;; https://skoolkid.github.io/rom/index.html
ROM_CLS            equ $0DAF
  
INPUT_SIZE_MAX         equ 30
ATTRIBUTE_FILE         equ $5800
AT                     equ $16
BLUE_ON_WHITE_BLINKING equ %11111001
BLACK_ON_GRAY          equ %00111000
CR                     equ $0D
  
  org $6000

start:
  call init
.loop:
  call read
  call eval
  call print
  jp   $
  jr   .loop

init:
  call ROM_CLS
  call header_print
  ret

read:
  call prompt_print
  call cursor_on
  ret

eval:
  ret

print:
  ret

header_print:
  ld   hl,header
  jp   hl_print

prompt_print:
  ld   hl,prompt
  call hl_print
  ret

hl_print: 
  ld   a,(hl)
  or   a
  ret  z
  call a_print
  inc  hl
  jr   hl_print

a_print:
  push af
  rst  $10
  pop  af
  cp   CR
  jp   z,cursor_cr
  jp   cursor_advance

cursor_cr:
  xor a
  ld  (cursor_x),a
  ld  a,(cursor_y)
  cp  23
  ret z
  inc a
  ld  (cursor_y),a
  ret

cursor_advance:
  ld   a,(cursor_x)
  cp   31
  jp   z,cursor_cr
  inc  a
  ld   (cursor_x),a
  ret

cursor_on:
  call cursor_attr_location_get
  ld   a,BLUE_ON_WHITE_BLINKING
  ld   (de),a
  ret

cursor_off:
  call cursor_attr_location_get
  ld   a,BLACK_ON_GRAY
  ld   (de),a
  ret

cursor_attr_location_get:
  ld   a,(cursor_y)
  ld   d,a
  ld   e,32
  mul  d,e
  ld   a,(cursor_x)
  add  de,a
  add  de,ATTRIBUTE_FILE
  ret

header:
  db   'NextLISP',CR,'Built ',__DATE__,' ', __TIME__,CR,0
prompt:
  db   CR,'>',0
cursor_y:
  db    0
cursor_x:
  db    0
input_size:
  db    0
input:
  block INPUT_SIZE_MAX,0
  db    0
