  DEVICE ZXSPECTRUMNEXT

  org $7E00

start:
  ld a,0
border:
  out $FE,a
  inc a
  and 7
  jr border

  SAVENEX OPEN "foo.nex",start,$FFFE,9
  SAVENEX BANK 5
  SAVENEX CLOSE
