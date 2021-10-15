	device     zxspectrumnext

dos_version	equ $0103
entry_bank	equ 0
stack		equ $c3f0
port_ula	equ $fe

	mmu 6, entry_bank, $c000
main:
	di
	xor	a
.set_border:
	out	(port_ula), a
	inc	a
	and	%00000111
	jr	.set_border

	savenex	open "nextlisp.nex", main, stack, entry_bank
	savenex auto
	savenex	close