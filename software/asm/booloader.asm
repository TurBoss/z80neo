	ORG 0000h          ; Start at address 0000h

	LD   A, 0AAh       ; 3E AA - Load the value 0xAA into the A register
	OUT  (80h), A      ; D3 80 - Output the value in A to I/O port 80h
	HALT               ; 76    - Halt the CPU (stop executing)
	NOP                ; 00    - No operation (filler/placeholder)
