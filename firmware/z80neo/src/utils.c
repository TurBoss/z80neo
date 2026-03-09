//
// Utilities
//


// unused
unsigned char reverseBits(unsigned char b) {
	return (b & 0b00000001) << 3 |
	       (b & 0b00000010) << 1 |
		   (b & 0b00000100) >> 1 |
		   (b & 0b00001000) >> 3 |
		   (b & 0b00010000) << 3 |
		   (b & 0b00100000) << 1 |
		   (b & 0b01000000) >> 1 |
		   (b & 0b10000000) >> 3;
}


// Bitwise OR
int setBit(int n, int k) {
	return (n | (1 << (k-1)));
}


// Bitwise AND
int clearBit(int n, int k) {
	return (n & ~(1 << (k-1)));
}


// Bitwise XOR
int toggleBit(int n, int k) {
	return (n ^ (1 << (k-1)));
}


// Bitwise AND
int checkBit(int n, int k) {
  return (n & (1 << (k-1))) != 0;
}

