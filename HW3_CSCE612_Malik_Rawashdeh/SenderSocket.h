// Malik Rawashdeh
// CSCE612
// HW3

#include "pch.h"

#define MAGIC_PORT 22345 // receiver listens on this port

#define MAX_SYN_ATTEMPTS 3 // maximum SYN attempts before declaring connection refused
#define MAX_ATTEMPTS 50 // max other attempts 

// possible status codes from ss.Open, ss.Send, ss.Close
#define STATUS_OK 0 // no error
#define ALREADY_CONNECTED 1 // second call to ss.Open() without closing connection
#define NOT_CONNECTED 2 // call to ss.Send()/Close() without ss.Open()
#define INVALID_NAME 3 // ss.Open() with targetHost that has no DNS entry
#define FAILED_SEND 4 // sendto() failed in kernel
#define TIMEOUT 5 // timeout after all retx attempts are exhausted
#define FAILED_RECV 6 // recvfrom() failed in kernel

#pragma once



class SenderSocket
{
private:
	SOCKET sock;
	// rto in ms 
	int rto;
	int estimated_rtt = 0;
	int dev_rtt = 0;
	int cnt_timeouts = 0;
	// save the target host sockaddr_in
	struct sockaddr_in target_addr;

	int last_status = STATUS_OK;

	clock_t constructor_start;



	// num fast ret
	int num_fast_retrans = 0;
	int num_retx = 0;

	// flow control
	int last_released = 0;


	// sender window arr of packets
	int W;
	int effective_window_size;

	Packet* pending_pkts;
	// next seq number
	DWORD next_seq;
	DWORD send_base; // first packet in the window
	DWORD nxt_send_seq; // next packet to send


	// thread sync mechanisms
	HANDLE empty;
	HANDLE full;
	HANDLE event_quit;
	HANDLE socket_receive_ready;

	thread stats_thread;
	thread worker_thread;

	void calculate_rto(clock_t sample_start) {
		// calculated in ms
		int sample_rtt = (float)(clock() - sample_start) / CLOCKS_PER_SEC * 1000;
		// printf("sample rtt %d\n", sample_rtt);
		// rto = estRTT + 4*devRTT
		// estRTT = 0.875*estRTT + 0.125*sampleRTT where alpha = 0.125
		// devRTT = 0.75*devRTT + 0.25*|sampleRTT - estRTT| where beta = 0.25
		// prevent devrtt frinm going below 10 ms
		estimated_rtt = 0.875 * estimated_rtt + 0.125 * sample_rtt;
		dev_rtt = 0.75 * dev_rtt + 0.25 * abs(sample_rtt - estimated_rtt);
		rto = estimated_rtt + 4 * max(dev_rtt, 10);

	}


	void WorkerRun();
	void StatsRun();

	bool has_pending_pkts() {
		return send_base < next_seq;
	}


public:
	SenderSocket();
	~SenderSocket();
	int Open(const char* target_host, int port, int sender_window, LinkProperties* lp);
	int Send(char* buf, int bytes);
	int Close(clock_t& end_send_time);
	int GetEstimatedRTT() {
		return estimated_rtt;
	}
	int GetWindowSizeBytes() {
		return (W * (MAX_PKT_SIZE - sizeof(SenderDataHeader)));
	}
};

