// HW3_CSCE612_Malik_Rawashdeh.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"

int main(int argc, char* argv[])
{

	// 7  cmd line arguments
	// 1. destination server (hostname or IP address)
	// 2. power of 2 buffer size to be transmitted in DWORDs
	// 3. sender window (in packets )
	// 4. round trip propogation delay ( in seconds)
	// 5 and 6. probobolity of loss in each direction
	// 7. the speed of the bottleneck link (in Mbps)

	if (argc != 8) {
		printf("Usage: HW3_CSCE612_Malik_Rawashdeh.exe \
				<destination server> <buffer size> <sender window> \
				<round trip propogation delay> <probobolity of loss in forward direction> \
				<probobolity of loss in backward direction> <speed of the bottleneck link>\n");
		return 1;
	}

	WSADATA wsaData;
	//Initialize WinSock; once per program run
	WORD wVersionRequested = MAKEWORD(2, 2);
	if (WSAStartup(wVersionRequested, &wsaData) != 0) {
		printf("WSAStartup error %d\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	// extrac the arguments
	const char* target_host = argv[1];
	int pow_buffer_size = atoi(argv[2]);
	int sender_window = atoi(argv[3]);
	float round_trip_propogation_delay = atof(argv[4]);
	float prob_loss_forward = atof(argv[5]);
	float prob_loss_return = atof(argv[6]);
	float link_bottle_neck = atof(argv[7]);





	// Main: sender W = 10, RTT 0.200 sec, loss 1e-05 / 0.0001, link 100 Mbps
	printf("Main: sender W = %d, RTT %.3f sec, loss %g / %g, link %.0f Mbps\n",
		sender_window, round_trip_propogation_delay, prob_loss_forward, prob_loss_return, link_bottle_neck);

	// need unique 

	// Main: initializing DWORD array with 2^20 elements... done in 0 ms 
	printf("Main: initializing DWORD array with 2^%d elements... ", pow_buffer_size);

	clock_t start = clock();
	// uint 64 buffer size
	UINT64 dword_buff_size = 1 << pow_buffer_size;
	DWORD* dword_buff = new DWORD[dword_buff_size]; // user requested buffer 
	for (UINT64 i = 0; i < dword_buff_size; i++) {
		dword_buff[i] = i; // required initialization
	}
	clock_t end = clock();
	int buff_init_time = (end - start) * 1000 / CLOCKS_PER_SEC;
	printf("done in %d ms\n", buff_init_time);

	Checksum cs;



	SenderSocket ss;

	LinkProperties lp;
	lp.RTT = round_trip_propogation_delay;
	lp.speed = link_bottle_neck * 1000000; // convert to bits/sec
	lp.pLoss[FORWARD_PATH] = prob_loss_forward;
	lp.pLoss[RETURN_PATH] = prob_loss_return;
	lp.bufferSize = sender_window + MAX_ATTEMPTS; // W + R (packats that can be in flight + packets that can be retransmitted)
	clock_t connect_start = clock();
	int ss_open_status = ss.Open(target_host, MAGIC_PORT, sender_window, &lp);
	if (ss_open_status != STATUS_OK) {
		printf("Main: connect failed with status %d\n", ss_open_status);
		return 1;
	}
	clock_t start_transfer_time = clock();

	float connect_time = (float)(clock() - connect_start) / CLOCKS_PER_SEC;
	// Main: connected to s3.irl.cs.tamu.edu in 2.237 sec, pkt size 1472 bytes 
	printf("Main: connected to %s in %.3f sec, pkt size %d bytes\n", target_host, connect_time, MAX_PKT_SIZE);

	char* char_buff = (char*)dword_buff;			// this buffer goes into the socket
	UINT64 byte_buff_size = dword_buff_size << 2;	// convert to bytes
	UINT64 off = 0;									// current position in the buffer
	int cnt = 0;									// number of packets sent
	while (off < byte_buff_size) {
		// decide the size of the next packet
		int bytes = min(byte_buff_size - off, MAX_PKT_SIZE - sizeof(SenderDataHeader));

		// send the packet
		int send_status = ss.Send(char_buff + off, bytes);
		// printf("Main: sent packet %d\n", cnt++);
		if (send_status != STATUS_OK) {
			printf("Main: send failed with status %d\n", send_status);
			return 1;
		}
		off += bytes;
	}



	printf("Main: closing connection...\n");
	clock_t end_send_time;
	int ss_close_status = ss.Close(end_send_time);
	if (ss_close_status != STATUS_OK) {
		printf("Main: close failed with status %d\n", ss_close_status);
		return 1;
	}


	// Main: transfer finished in 0.010 sec 
	float transfer_time = (float)(end_send_time - start_transfer_time) / CLOCKS_PER_SEC;
	// rate of send in Kbps
	float rate = (float)(byte_buff_size * 8) / transfer_time / 1000;
	DWORD check = cs.CRC32((unsigned char*)dword_buff, dword_buff_size << 2);
	printf("Main:\ttransfer finished in %.3f sec, %.2f Kbps, checksum %X\n", transfer_time, rate, check);
	float estimated_rtt = (float)ss.GetEstimatedRTT() / 1000.0;
	int window_size_bytes = ss.GetWindowSizeBytes();
	float ideal_rate = (float)window_size_bytes * 8.0 / (float)estimated_rtt / 1000.0;
	printf("Main:\testRTT %.3f, ideal rate %.2f Kbps\n", estimated_rtt, ideal_rate);
	// 
	WSACleanup();

	return 0;
}


// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
