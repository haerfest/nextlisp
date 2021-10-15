	device zxspectrumnext

dos_version				equ $0103
entry_bank				equ 0
stack					equ $c3f0
tile_definitions_offset			equ $5D00 - $4000  ; 1,024 bytes at [$5D00, $6100)
tilemap_offset				equ $6100 - $4000  ; 2,560 bytes at [$6100, $6B00)

port_ula				equ $fe
reg_clip_window_tilemap			equ $1b
reg_tilemap_x_scroll_msb		equ $2f
reg_tilemap_x_scroll_lsb		equ $30
reg_tilemap_offset_y			equ $31
reg_palette_index			equ $40
reg_palette_value			equ $41
reg_palette_control			equ $43
reg_tilemap_transparency_index		equ $4c
reg_mmu_slot_2				equ $52
reg_ula_control				equ $68
reg_display_control			equ $69
reg_tilemap_control			equ $6b
reg_tilemap_base_address		equ $6e
reg_tile_definitions_base_address	equ $6f

	mmu 6, entry_bank, $c000

; -----------------------------------------------------------------------------
; Main program.
; -----------------------------------------------------------------------------
main:
	call	init_tilemap
	call	cls
	call	banner

	jp	$

; -----------------------------------------------------------------------------
; Initialises the tilemap.
; -----------------------------------------------------------------------------
init_tilemap:
	nextreg reg_tilemap_x_scroll_msb, 0
	nextreg reg_tilemap_x_scroll_lsb, 0
	nextreg reg_tilemap_offset_y, 0
	nextreg	reg_clip_window_tilemap, 0
	nextreg	reg_clip_window_tilemap, 159
	nextreg	reg_clip_window_tilemap, 0
	nextreg	reg_clip_window_tilemap, 255
	nextreg reg_tilemap_transparency_index, $0f
	nextreg reg_ula_control, %10100000
	nextreg reg_display_control, 0
	nextreg reg_tilemap_control, %10001001
	
	; Initialise the tilemap palette.
	nextreg reg_palette_control, %00110000
	nextreg reg_palette_index, 0
	nextreg reg_palette_value, %10110110  ; background = gray
	nextreg reg_palette_value, %00000000  ; foreground = black

	; Load the tilemap font.
	nextreg reg_tilemap_base_address, tilemap_offset >> 8
	nextreg reg_tile_definitions_base_address, tile_definitions_offset >> 8

	nextreg reg_mmu_slot_2, 10  ; ensure bank 5 is banked in
	ld	hl, .font
	ld	de, $4000 + tile_definitions_offset + ' ' * 8
	ld	bc, 768
	ldir

	ret

.font:
	binary "font.bin"

; -----------------------------------------------------------------------------
; Shows a banner.
;
; Destroys: AF, DE, HL
; -----------------------------------------------------------------------------
banner:
	ld	hl, .banner
	ld	de, $4000 + tilemap_offset  ; assumes bank 5 banked in
.loop:
	ld	a, (hl)
	or	a
	ret	z
	ld	(de), a
	inc	hl
	inc	de
	inc	de  ; skip attribute byte
	jr	.loop

.banner:
	db	"NextLISP (", __DATE__, " ", __TIME__, ")", 0

; -----------------------------------------------------------------------------
; Clears the tilemap screen.
; -----------------------------------------------------------------------------
cls:
	push	af, bc, hl
	
	ld	hl, $4000 + tilemap_offset
	ld	bc, 32 * 80 * 2
.loop:
	ld	(hl), ' '
	inc	hl
	ld	(hl), 1  ; white
	inc	hl
	dec	bc
	ld	a, b
	or	c
	jr	nz, .loop
	
	pop	hl, bc, af
	ret

	savenex	open "nextlisp.nex", main, stack, entry_bank
	savenex auto
	savenex	close