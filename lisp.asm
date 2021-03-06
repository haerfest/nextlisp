	device zxspectrumnext

columns					equ 80
rows					equ 32

left					equ $08
right					equ $09
down					equ $0a
up					equ $0b
backspace				equ $0c
newline					equ $0d

cursor_blink_interval			equ 15
entry_bank				equ 0
port_ula				equ $fe
rom_keyboard				equ $02bf
stack					equ $dfff

reg_clip_window_tilemap			equ $1b
reg_display_control			equ $69
reg_mmu_slot_2				equ $52
reg_palette_control			equ $43
reg_palette_index			equ $40
reg_palette_value			equ $41
reg_tile_definitions_base_address	equ $6f
reg_tilemap_base_address		equ $6e
reg_tilemap_control			equ $6b
reg_tilemap_offset_y			equ $31
reg_tilemap_transparency_index		equ $4c
reg_tilemap_x_scroll_lsb		equ $30
reg_tilemap_x_scroll_msb		equ $2f
reg_ula_control				equ $68

sysvars_errnr				equ $5c3a
sysvars_flags				equ $5c3b
sysvars_last_k				equ $5c08

tile_definitions_offset			equ $5D00 - $4000  ; 1,024 bytes at [$5D00, $6100).
tilemap_offset				equ $6100 - $4000  ; 2,560 bytes at [$6100, $6B00).

	mmu 6, entry_bank, $c000

; -----------------------------------------------------------------------------
; Main program.
; -----------------------------------------------------------------------------
main:
	call	init_irq
	call	init_tilemap
	call	cls
	call	banner

.loop:
	call	read
	jr	.loop

; -----------------------------------------------------------------------------
; ?
; -----------------------------------------------------------------------------
read:
	; Check if a new key has been pressed.
	ld	hl, sysvars_flags
	bit	5, (hl)
	ret	z
	
	; Signal that we read the key.
	res	5, (hl)

	; Echo the key.
	ld	a, (sysvars_last_k)
	call	print_char
	ret

; -----------------------------------------------------------------------------
; Takes over the IRQ handling.
; -----------------------------------------------------------------------------
init_irq:
	di

	; IRQ handler at $CFCF is a single JP .irq.
	ld	a, $c3
	ld	($cfcf), a
	ld	hl, .irq
	ld	($cfd0), hl

	; Store pointers to IRQ handler at [$CE00, $CF00].
	ld	hl, $ce00
	ld	(hl), $cf
	ld	de, $ce01
	ld	bc, 256
	ldir

	; Switch to IM 2.
	ld	a, $ce
	ld	i, a
	im	2
	
	ei
	ret

.irq:
	push	af, bc, de, hl

	; Decrement cursor blink counter and test for zero.
	ld	hl, .cursor_blink_counter
	dec	(hl)
	jr	nz, .scan_keyboard
	
	; Reset the cursor blink counter and blink the cursor.
	ld	(hl), cursor_blink_interval
	call	.blink_cursor

.scan_keyboard:
	push	iy
	ld	iy, sysvars_errnr
	; TODO Ensure ROM 3 (48K BASIC) is mapped.
	;      Ensure bank 5 is mapped.
	call	rom_keyboard
	pop	iy

	pop	hl, de, bc, af
	ei
	reti

; -----------------------------------------------------------------------------
; Counter used to blink the cursor at a fixed rate.
; -----------------------------------------------------------------------------
.cursor_blink_counter:
	db	cursor_blink_interval

; -----------------------------------------------------------------------------
; Blinks the cursor by changing a cell's palette offset.
;
; Destroys: AF, DE, HL.
; -----------------------------------------------------------------------------
.blink_cursor:
	; Toggle the cursor blink state between palette offsets 2 and 6.
	ld	a, (cursor_palette_offset)
	or	2
	xor	4
	ld	(cursor_palette_offset), a

	; Point to the attribute cell at the current cursor location.
	ld	hl, (cursor_offset)
	inc	hl

	; Set the attribute cell.
	ld	(hl), a
	ret

; -----------------------------------------------------------------------------
; Initialises the tilemap.
; -----------------------------------------------------------------------------
init_tilemap:
	nextreg reg_tilemap_x_scroll_msb,       0
	nextreg reg_tilemap_x_scroll_lsb,       0
	nextreg reg_tilemap_offset_y,           0
	nextreg	reg_clip_window_tilemap,        0
	nextreg	reg_clip_window_tilemap,        159
	nextreg	reg_clip_window_tilemap,        0
	nextreg	reg_clip_window_tilemap,        255
	nextreg reg_tilemap_transparency_index, $0f
	nextreg reg_ula_control,                %10100000
	nextreg reg_display_control,            0
	if columns == 40
	nextreg reg_tilemap_control,            %10001001
	else
	nextreg reg_tilemap_control,            %11001001
	endif
	
	; Initialise the tilemap palette.
	nextreg reg_palette_control, %00110000
	nextreg reg_palette_index,   0
	nextreg reg_palette_value,   %10110110  ; 0 = gray   \ normal
	nextreg reg_palette_value,   %00000000  ; 1 = black  / text
	nextreg reg_palette_value,   %00000011  ; 2 = blue   \ cursor
	nextreg reg_palette_value,   %11111111  ; 3 = white  / blink 1
	nextreg reg_palette_value,   %00000000  ; 4 = black  \ dummy
	nextreg reg_palette_value,   %00000000  ; 5 = black  /
	nextreg reg_palette_value,   %11111111  ; 6 = white  \ cursor
	nextreg reg_palette_value,   %00000000  ; 7 = black  / blink 2

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
.loop:
	ld	a, (hl)
	or	a
	ret	z
	push	hl
	call	print_char
	pop	hl
	inc	hl
	jr	.loop

.banner:
	db	"NextLISP (", __DATE__, " ", __TIME__, ")", $0d, $0d, 0

; -----------------------------------------------------------------------------
; Prints a character to the screen at the cursor coordinates and advances
; the cursor.
;
; Input: A = character to print.
;
; Destroys: BC, DE, HL.
; -----------------------------------------------------------------------------
print_char:
	; Handle special characters separately.
	cp	newline
	jp	z, .print_newline
	cp	backspace
	jp	z, .print_backspace
	cp	up
	jp	z, .print_up
	cp	down
	jp	z, .print_down
	cp	left
	jp	z, .print_left
	cp	right
	jp	z, .print_right
	cp	' '
	ret	s

	; Store character and advance cursor.
	ld	hl, (cursor_offset)
	ld	(hl), a
	call	.advance_cursor
	ret

; -----------------------------------------------------------------------------
; Advances the cursor one position.
;
; TODO: Scrolling.
; -----------------------------------------------------------------------------
.advance_cursor:	
	; Advance cursor X, check for end of line.
	ld	a, (cursor_x)
	cp	columns - 1
	jr	z, .print_newline
	
	call	.hide_cursor

	; Not end of line, increase and store cursor X.
	inc	a
	ld	(cursor_x), a

	; Update the cursor offset.
	inc	hl
	ld	(cursor_offset), hl
	
	call	.show_cursor
	ret

; -----------------------------------------------------------------------------
; Clears the cursor.
;
; Destroys: HL.
; -----------------------------------------------------------------------------
.hide_cursor:
	; Reset the attribute cell.
	ld	hl, (cursor_offset)
	inc	hl
	ld	(hl), 0
	ret

; -----------------------------------------------------------------------------
; Shows the cursor.
;
; Destroys: A, HL.
; -----------------------------------------------------------------------------
.show_cursor:
	; Set the attribute cell.
	ld	a, (cursor_palette_offset)
	ld	hl, (cursor_offset)
	inc	hl
	ld	(hl), a
	ret

; -----------------------------------------------------------------------------
; Prints a newline and advances the cursor.
;
; TODO: Scrolling.
; -----------------------------------------------------------------------------
.print_newline:
	call	.hide_cursor

	; Set cursor X to beginning of line.
	xor	a
	ld	(cursor_x), a
	
	; Advance cursor Y by one line, back to top when bottom reached.
	ld	a, (cursor_y)
	inc	a
	cp	rows
	jr	nz, .store_y
	xor	a
.store_y:
	ld	(cursor_y), a
	call	.update_cursor_offset
	
	call	.show_cursor
	ret

; -----------------------------------------------------------------------------
; Handles a backspace character.
; -----------------------------------------------------------------------------
.print_backspace:
	call	.hide_cursor

	; Move cursor one location back.
	ld	a, (cursor_x)
	or	a
	jr	z, .at_beginning_of_line
	dec	a
	ld	(cursor_x), a
	jr	.update_and_erase

.at_beginning_of_line:
	; Cursor is at beginning of a line, move one line up.
	ld	a, (cursor_y)
	or	a
	jr	nz, .to_end_of_previous_line
	ld	a, rows
.to_end_of_previous_line:
	dec	a
	ld	(cursor_y), a
	
	; Find end of text on this line.
	ld	a, columns - 1
	ld	(cursor_x), a
.search:
	; Do we have a non-space here?
	call	.update_cursor_offset
	ld	hl, (cursor_offset)
	ld	a, (hl)
	cp	' '
	jr	nz, .erase

	; No, decrease cursor X and keep searching.
	ld	a, (cursor_x)
	dec	a
	ld	(cursor_x), a
	jr	nz, .search

.update_and_erase:
	call	.update_cursor_offset
	call	.show_cursor
.erase:
	ld	hl, (cursor_offset)
	ld	a, ' '
	ld	(hl), a

	ret

.print_up:
	ld	a, (cursor_y)
	or	a
	ret	z
	call	.hide_cursor
	dec	a
	ld	(cursor_y), a
	call	.update_cursor_offset
	jp	.show_cursor

.print_down:
	ld	a, (cursor_y)
	cp	rows - 1
	ret	z
	call	.hide_cursor
	inc	a
	ld	(cursor_y), a
	call	.update_cursor_offset
	jp	.show_cursor

.print_left:
	ld	a, (cursor_x)
	or	a
	ret	z
	call	.hide_cursor
	dec	a
	ld	(cursor_x), a
	call	.update_cursor_offset
	jp	.show_cursor

.print_right:
	ld	a, (cursor_x)
	cp	columns - 1
	ret	z
	call	.hide_cursor
	inc	a
	ld	(cursor_x), a
	call	.update_cursor_offset
	jp	.show_cursor

; -----------------------------------------------------------------------------
; Updates cursor_offset from cursor_{x,y}.
;
; Destroys: BC, DE, HL.
; -----------------------------------------------------------------------------
.update_cursor_offset:
	; Multiply cursor Y by columns and two cells (character + attribute).
	ld	hl, cursor_y
	ld	d, (hl)
	ld	e, columns * 2
	mul	de

	; Multiply cursor X by two cells.
	ld	hl, cursor_x
	ld	b, 0
	ld	c, (hl)
	sla	c

	; Add both to the tilemap address and store.
	ld	hl, $4000 + tilemap_offset
	add	hl, de
	add	hl, bc
	ld	(cursor_offset), hl
	ret

; -----------------------------------------------------------------------------
; Clears the tilemap screen.
; -----------------------------------------------------------------------------
cls:
	push	af, bc, hl
	
	ld	hl, $4000 + tilemap_offset
	ld	bc, rows * columns * 2
.loop:
	ld	(hl), ' '
	inc	hl
	ld	(hl), 0
	inc	hl
	dec	bc
	ld	a, b
	or	c
	jr	nz, .loop
	
	pop	hl, bc, af
	ret

; -----------------------------------------------------------------------------
; Cursor blink state, zero or one.
; -----------------------------------------------------------------------------
cursor_palette_offset
	db	0
; -----------------------------------------------------------------------------
; Cursor Y position [0, rows).
; -----------------------------------------------------------------------------
cursor_y:
	db	0
; -----------------------------------------------------------------------------
; Cursor X position [0, columns).
; -----------------------------------------------------------------------------
cursor_x:
	db	0
; -----------------------------------------------------------------------------
; Cursor offset in bank 5, always synchronised with cursor_{x,y}.
; -----------------------------------------------------------------------------
cursor_offset:
	dw	$4000 + tilemap_offset

	savenex	open "nextlisp.nex", main, stack, entry_bank
	savenex auto
	savenex	close
