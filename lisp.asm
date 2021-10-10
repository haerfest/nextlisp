  ;; To assemble:
  ;;
  ;;   $ sjasmplus --raw=lisp.bin lisp.asm
  ;; 
  ;; To execute from BASIC:
  ;;
  ;;   10 CLEAR %$5FFF
  ;;   20 LOAD "lisp.bin" CODE %$6000
  ;;   30 RANDOMIZE % USR $6000

ROM_CHARACTER_SET            equ $3D00
TILEMAP_ADDR                 equ $4000 + 1024

NEXTREG_TILEMAP_CLIP         equ $1B
NEXTREG_TILEMAP_OFFSET_X_MSB equ $2F
NEXTREG_TILEMAP_OFFSET_X     equ $30
NEXTREG_TILEMAP_OFFSET_Y     equ $31
NEXTREG_PALETTE_INDEX        equ $40
NEXTREG_PALETTE_VALUE        equ $41
NEXTREG_ENHANCED_ULA_CTRL    equ $43
NEXTREG_ULA_CTRL             equ $68
NEXTREG_TILEMAP_CONTROL      equ $6B
NEXTREG_TILEMAP_DEFAULT_ATTR equ $6C
NEXTREG_TILEMAP_MAP_ADDR     equ $6E 
NEXTREG_TILEMAP_DEF_ADDR     equ $6F

  device zxspectrumnext

  org $6000

main:
  call init
  call print_banner
  jp   $


init:
  call init_tilemap
  ret

init_tilemap:
  nextreg NEXTREG_ULA_CTRL,%10000000
  nextreg NEXTREG_TILEMAP_DEF_ADDR,     $00 ; $4000
  nextreg NEXTREG_TILEMAP_MAP_ADDR,     (TILEMAP_ADDR - $4000) >> 8
  nextreg NEXTREG_TILEMAP_DEFAULT_ATTR, %00000000
  nextreg NEXTREG_TILEMAP_CLIP,         0
  nextreg NEXTREG_TILEMAP_CLIP,         159
  nextreg NEXTREG_TILEMAP_CLIP,         0
  nextreg NEXTREG_TILEMAP_CLIP,         255
  nextreg NEXTREG_TILEMAP_OFFSET_X_MSB, 0
  nextreg NEXTREG_TILEMAP_OFFSET_X,     0
  nextreg NEXTREG_TILEMAP_OFFSET_Y,     0
  nextreg NEXTREG_TILEMAP_CONTROL,      %10101001

  ;; Select tilemap palette.
  nextreg NEXTREG_ENHANCED_ULA_CTRL, %00110000
  nextreg NEXTREG_PALETTE_INDEX,     0
  nextreg NEXTREG_PALETTE_VALUE,     %00000000 ; #0 = black
  nextreg NEXTREG_PALETTE_VALUE,     %11111111 ; #1 = white

  ;; Copy the ROM character set, which starts at character 32 (space).
  ;; The 128 characters take up 128 x 8 = 1024 = $400 bytes.
  ld  hl,ROM_CHARACTER_SET
  ld  de,$4000 + 32 * 8
  ld  bc,$4000 - $3D00
  ldir

  ret

print_hl_at_bc:
  ;; Make DE point to TILEMAP_ADDR[B][C]. 
  ld  d,b
  ld  e,32
  mul de
  ld  a,c
  add de,a
  add de,TILEMAP_ADDR

  ;; Print each character until zero byte.
.print:
  ld  a,(hl)
  or  a
  ret z
  ld  (de),a
  inc hl
  inc de
  jr  .print
  
print_banner:
  ld  hl,banner
  ld  bc,0
  jp  print_hl_at_bc

banner:
  db "NextLISP (Built ", __DATE__, " ", __TIME__, ")",0
