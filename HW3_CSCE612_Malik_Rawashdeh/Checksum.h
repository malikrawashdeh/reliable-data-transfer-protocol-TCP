#pragma once
class Checksum
{
private:
	DWORD crc32_table[256];
public:
	Checksum();
	~Checksum();
	
	DWORD CRC32(unsigned char* buf, size_t len);
};

