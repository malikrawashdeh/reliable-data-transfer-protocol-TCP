#include "pch.h"


#define FORWARD_PATH 0
#define RETURN_PATH 1
#define MAGIC_PROTOCOL 0x8311AA 
#define MAX_PKT_SIZE (1500-28) // maximum UDP packet size accepted by receiver


#pragma once 
#pragma pack(push, 1)



class Flags {
public:
	DWORD reserved : 5; // must be zero
	DWORD SYN : 1;
	DWORD ACK : 1;
	DWORD FIN : 1;
	DWORD magic : 24;
	Flags() { memset(this, 0, sizeof(*this)); magic = MAGIC_PROTOCOL; }
};

class SenderDataHeader {
public:
	Flags flags;
	DWORD seq; // must begin from 0
};

class LinkProperties {
public:
	// transfer parameters
	float RTT; // propagation RTT (in sec)
	float speed; // bottleneck bandwidth (in bits/sec)
	float pLoss[2]; // probability of loss in each direction
	DWORD bufferSize; // buffer size of emulated routers (in packets)
	LinkProperties() { memset(this, 0, sizeof(*this)); }
};

class SenderSynHeader {

public:
	SenderDataHeader sdh;
	LinkProperties lp;
};

// responses

class ReceiverHeader {
public:
	Flags flags;
	DWORD recvWnd; // receiver window for flow control (in pkts)
	DWORD ackSeq; // ack value = next expected sequence
};

class Packet {
public:
	int type; // SYN, FIN, data
	int size; // bytes in packet data
	clock_t txTime; // transmission time
	char pkt[MAX_PKT_SIZE]; // packet with header
};

#pragma pack(pop)