fourbfiveb = (
	0b11110, # Hex data 0
	0b01001, # Hex data 1
	0b10100, # Hex data 2
	0b10101, # Hex data 3
	0b01010, # Hex data 4
	0b01011, # Hex data 5
	0b01110, # Hex data 6
	0b01111, # Hex data 7
	0b10010, # Hex data 8
	0b10011, # Hex data 9
	0b10110, # Hex data A
	0b10111, # Hex data B
	0b11010, # Hex data C
	0b11011, # Hex data D
	0b11100, # Hex data E
	0b11101  # Hex data F
)

ctrlchars = (
	0b00000, # Quiet (Signalverlust)
	0b11111, # Idle (Pause)
	0b11000, # Start #1
	0b10001, # Start #2
	0b01101, # End
	0b00111, # Reset
	0b11001, # Set
	0b00100  # Halt
)

fivebfourb = (
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	0,
	1,
	4,
	5,
	0,
	0,
	6,
	7,
	0,
	0,
	8,
	9,
	2,
	3,
	10,
	11,
	0,
	0,
	12,
	13,
	14,
	15,
	0,
	0
)

def decode(x):
	global fivebfourb
	return fivebfourb[x]
