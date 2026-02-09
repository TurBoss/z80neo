//
// Utilities
//


// Bitwise OR
int setBit(int n, int k) {
	return (n | (1 << (k - 1)));
}

// Bitwise AND
int clearBit(int n, int k) {
	return (n & (~(1 << (k - 1))));
}

// Bitwise XOR
int toggleBit(int n, int k) {
	return (n ^ (1 << (k - 1)));
}

// Bitwise AND
int checkBit(int n, int k) {
  return (n & (1 << (k - 1))) != 0;
}