#include "pch.h"
#include "Checksum.h"

Checksum::Checksum()
{
	// set up a look up table for the CRC32
	for (int i = 0; i < 256; i++) {
		DWORD c = i;
		for (int j = 0; j < 8; j++) {
			c = (c & 1) ? (0xedb88320 ^ (c >> 1)) : (c >> 1);
		}
		crc32_table[i] = c;
	}
}

Checksum::~Checksum()
{
}

DWORD Checksum::CRC32(unsigned char* buf, size_t len)
{
	DWORD c = 0xffffffff;
	for (size_t i = 0; i < len; i++) {
		c = crc32_table[(c ^ buf[i]) & 0xff] ^ (c >> 8);
	}
	return c ^ 0xffffffff;
}