#include "pch.h"
#include "SenderSocket.h"



SenderSocket::SenderSocket()
{
	// set sock to invalid socket
	sock = INVALID_SOCKET;
	// clear the target_addr
	memset(&target_addr, 0, sizeof(target_addr));
	rto = 1000; // 1 sec
	constructor_start = clock();

	next_seq = 0;
	pending_pkts = nullptr;

	// create the 
	event_quit = CreateEvent(NULL, true, false, NULL);




	// create the stats thread
	stats_thread = thread(&SenderSocket::StatsRun, this);



}

SenderSocket::~SenderSocket()
{
	// join the threads
	// quit event
	SetEvent(event_quit);
	if (worker_thread.joinable()) {
		worker_thread.join();
	}
	if (worker2_thread.joinable()) {
		worker2_thread.join();
	}
	if (stats_thread.joinable()) {
		stats_thread.join();
	}


	// best practice 
	if (sock != INVALID_SOCKET) {
		closesocket(sock);
	}

	if (pending_pkts != nullptr) {
		delete[] pending_pkts;
		pending_pkts = nullptr;
	}

	if (event_quit != NULL) {
		CloseHandle(event_quit);
		event_quit = NULL;
	}

	if (empty != NULL) {
		CloseHandle(empty);
		empty = NULL;
	}

	if (full != NULL) {
		CloseHandle(full);
		full = NULL;
	}



}

int SenderSocket::Open(const char* target_host, int port, int sender_window, LinkProperties* lp)
{
	// check if already connected
	if (sock != INVALID_SOCKET) {
		return ALREADY_CONNECTED;
	}

	// create sock
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock == INVALID_SOCKET) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		WSACleanup();
		return false;
	}

	// bind to 0
	struct sockaddr_in local_addr;
	local_addr.sin_family = AF_INET;
	local_addr.sin_port = htons(0);
	local_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);

	if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) == SOCKET_ERROR) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		return FAILED_SEND;
	}

	// get the address of the target host
	struct hostent* target;

	// fill in the address structure
	// clear
	memset(&target_addr, 0, sizeof(target_addr));
	// first assume the target is an IP address

	DWORD IP = inet_addr(target_host);
	if (IP == INADDR_NONE)
	{
		// if not a valid IP, then do a DNS lookup
		if ((target = gethostbyname(target_host)) == NULL)
		{
			// [0.002] --> target s38.irl.cs.tamu.edu is invalid 
			printf("[%.3f] --> target %s is invalid\n",
				(float)(clock() - constructor_start) / CLOCKS_PER_SEC, target_host);
			return INVALID_NAME;
		}
		else {
			// fill in the address structure
			memcpy(&target_addr.sin_addr, target->h_addr_list[0], target->h_length);
		}
	}
	else
	{
		// if a valid IP, directly drop its binary version into sin_addr
		target_addr.sin_addr.S_un.S_addr = IP;
	}

	target_addr.sin_port = htons(port);
	target_addr.sin_family = AF_INET; // udp

	int sq = 0;
	SenderSynHeader syn_pkt;
	syn_pkt.lp = *lp;
	syn_pkt.sdh.seq = sq;
	syn_pkt.sdh.flags.SYN = 1;

	int ret;
	// rto is  maximum of 1 second and 2*lp.RTT
	rto = max(1000, (int)(2 * lp->RTT * 1000));

	bool synack_received = false;
	for (int i = 0; i < MAX_SYN_ATTEMPTS; i++) {

		// [ 0.002] --> SYN 0 (attempt 1 of 3, RTO 1.000) to 128.194.135.82
		clock_t end = clock();
		float cur_sec_elapsed = (float)(end - constructor_start) / CLOCKS_PER_SEC;
		float rto_sec = (float)rto / 1000;
		// printf("[ %.3f] --> SYN %d (attempt %d of %d, RTO %.3f) to %s\n", cur_sec_elapsed, sq, i + 1, MAX_SYN_ATTEMPTS, rto_sec, target_host);
		clock_t start_time_cur_pkt = clock();
		// send the SYN
		int bytes = sendto(sock, (char*)&syn_pkt, sizeof(syn_pkt), 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
		if (bytes == SOCKET_ERROR) {
			// [0.001] --> failed sendto with 10049 
			printf("[%.3f] --> failed sendto with %d\n",
				(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
			return FAILED_SEND;
		}
		// timer for RTO of 1 sec
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		struct timeval rto_timer;
		// set rto_timer to rto 
		rto_timer.tv_sec = (int)rto / 1000;
		rto_timer.tv_usec = ((int)rto % 1000) * 1000; // calculates the microseconds

		int cur_time_elapsed = 0;
		clock_t start_time_cur_run = clock();

		while (true) {

			// recaculate the rto_timer based on the time remaining
			int time_remaining = rto - cur_time_elapsed;

			if (time_remaining <= 0) {
				// timeout
				break;
			}

			rto_timer.tv_sec = (int)time_remaining / 1000;
			rto_timer.tv_usec = ((int)time_remaining % 1000) * 1000; // calculates the microseconds

			if (ret = select(0, &fds, NULL, NULL, &rto_timer) == 0) {
				// timeout
				break;
			}
			else if (ret == SOCKET_ERROR) {
				printf("[%.3f] --> failed select with %d\n",
					(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
				return FAILED_SEND;
			}

			// receive the SYNACK




			ReceiverHeader synack_pkt;
			int addr_len = sizeof(target_addr);
			bytes = recvfrom(sock, (char*)&synack_pkt, sizeof(synack_pkt), 0, (struct sockaddr*)&target_addr, &addr_len);
			// check if timeout

			if (bytes == SOCKET_ERROR) {
				// [0.003] <-- failed recvfrom with 10054
				printf("[%.3f] <-- failed recvfrom with %d\n",
					(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
				return FAILED_RECV;
			}

			if (synack_pkt.flags.SYN && synack_pkt.flags.ACK && synack_pkt.ackSeq == sq) {
				// SYNACK received
				synack_received = true;
				// set rto to 3 * rtt
				float cur_sec_elapsed = (float)(clock() - start_time_cur_pkt) / CLOCKS_PER_SEC;
				// [ 0.223] <-- SYN-ACK 0 window 1; setting initial RTO to 0.663 
				rto = 3 * (int)(cur_sec_elapsed * 1000);
				float rto_sec = (float)rto / 1000;
				// printf("[ %.3f] <-- SYN-ACK %d window %d; setting initial RTO to %.3f\n",
				// (float)(clock() - constructor_start) / CLOCKS_PER_SEC, synack_pkt.ackSeq, synack_pkt.recvWnd, rto_sec);
				estimated_rtt = rto;
				last_released = min(sender_window, synack_pkt.recvWnd);
				break;
			}
			else {
				// bogus packet
				cur_time_elapsed = (clock() - start_time_cur_run) * 1000 / CLOCKS_PER_SEC;
				continue;
			}
		}

		if (synack_received) {
			break;
		}
	}

	if (!synack_received) {
		return TIMEOUT;
	}
	// set the window size
	W = sender_window;
	effective_window_size = last_released;
	// create the sender window
	pending_pkts = new Packet[sender_window];
	// create the semaphores
	// last_recv is the number of empty slots you can send 
	empty = CreateSemaphore(NULL, effective_window_size, W, NULL);
	full = CreateSemaphore(NULL, 0, W, NULL);
	// edge case single packet 
	single_pkt_case = CreateEvent(NULL, false, false, NULL);
	if (empty == NULL || full == NULL) {
		return FAILED_SEND;
	}



	// set the send base
	send_base = 0;
	// set the next send seq
	nxt_send_seq = 0;



	// create the worker threads
	worker_thread = thread(&SenderSocket::WorkerRun, this);
	worker2_thread = thread(&SenderSocket::Worker2Run, this);

	return STATUS_OK;
}


int SenderSocket::Close(clock_t& end_send_time)
{
	if (sock == INVALID_SOCKET) {
		return NOT_CONNECTED;
	}
	if (last_status != STATUS_OK) {
		return last_status;
	}

	// signal quite event 

	SetEvent(event_quit);
	// wait for the worker thread to finish
	if (worker_thread.joinable()) {
		worker_thread.join();
	}
	if (worker2_thread.joinable()) {
		worker2_thread.join();
	}
	// wait for the stats thread to finish
	if (stats_thread.joinable()) {
		stats_thread.join();
	}
	end_send_time = clock();

	// send FIN
	SenderDataHeader fin_pkt;
	fin_pkt.flags.FIN = 1;
	fin_pkt.seq = next_seq;

	bool finack_received = false;
	int ret;
	for (int i = 0; i < MAX_ATTEMPTS; i++) {
		// [ 0.233] --> FIN 0 (attempt 1 of 5, RTO 0.663) 
		float cur_sec_elapsed = (float)(clock() - constructor_start) / CLOCKS_PER_SEC;
		float rto_sec = (float)rto / 1000;
		// printf("[ %.3f] --> FIN %d (attempt %d of %d, RTO %.3f)\n", cur_sec_elapsed, fin_pkt.seq, i + 1, MAX_ATTEMPTS, rto_sec);

		// send the FIN
		int bytes = sendto(sock, (char*)&fin_pkt, sizeof(fin_pkt), 0, (struct sockaddr*)&target_addr, sizeof(target_addr));

		if (bytes == SOCKET_ERROR) {
			printf("[%.3f] --> failed sendto with %d\n",
				(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
			return FAILED_SEND;
		}

		// timer for RTO of 1 sec
		fd_set fds;
		FD_ZERO(&fds);
		FD_SET(sock, &fds);

		struct timeval rto_timer;
		// set rto_timer to rto 

		rto_timer.tv_sec = (int)rto / 1000;
		rto_timer.tv_usec = ((int)rto % 1000) * 1000; // calculates the microseconds


		int cur_time_elapsed = 0;
		clock_t start_time_cur_run = clock();

		// use loop to detect bogus packets
		while (true) {

			// recaculate the rto_timer based on the time remaining
			int time_remaining = rto - cur_time_elapsed;

			if (time_remaining <= 0) {
				// timeout
				break;
			}

			rto_timer.tv_sec = (int)time_remaining / 1000;
			rto_timer.tv_usec = ((int)time_remaining % 1000) * 1000; // calculates the microseconds

			if (ret = select(0, &fds, NULL, NULL, &rto_timer) == 0) {
				// timeout
				break;
			}
			else if (ret == SOCKET_ERROR) {
				printf("[%.3f] <-- failed select with %d\n",
					(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
				return FAILED_SEND;
			}

			// receive the FINACK

			ReceiverHeader finack_pkt;
			int addr_len = sizeof(target_addr);
			bytes = recvfrom(sock, (char*)&finack_pkt, sizeof(finack_pkt), 0, (struct sockaddr*)&target_addr, &addr_len);
			if (bytes == SOCKET_ERROR) {
				printf("[%.3f] <-- failed recvfrom with %d\n",
					(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
				return FAILED_RECV;
			}

			if (finack_pkt.flags.FIN && finack_pkt.flags.ACK && finack_pkt.ackSeq == fin_pkt.seq) {
				// FINACK received
				finack_received = true;
				// [ 0.457] <-- FIN-ACK 0 window 0 
				cur_sec_elapsed = (float)(clock() - constructor_start) / CLOCKS_PER_SEC;
				printf("[ %.3f] <-- FIN-ACK %d window %X\n", cur_sec_elapsed, finack_pkt.ackSeq, finack_pkt.recvWnd);
				break;
			}
			else {
				// bogus packet
				cur_time_elapsed = (clock() - start_time_cur_run) * 1000 / CLOCKS_PER_SEC;
				continue;
			}
		}

		if (finack_received) {
			break;
		}

	}

	if (!finack_received) {
		return TIMEOUT;
	}

	closesocket(sock);
	sock = INVALID_SOCKET;
	return STATUS_OK;
}


void SenderSocket::StatsRun() {
	//printf("StatsRun: entered\n");
	int last_base = 0;

	while (true) {
		int ret = WaitForSingleObject(event_quit, 2000);
		if (ret == WAIT_OBJECT_0) {
			//printf("StatsRun: exiting\n");
			return;
		}
		// print stats using 
		// Print the formatted statistics
		// in MB
		unsigned long long sent_bytes = send_base * (MAX_PKT_SIZE - sizeof(SenderDataHeader));
		float goodput = (float)(send_base - last_base) * (MAX_PKT_SIZE - sizeof(SenderDataHeader)) * 8.0 / 1000000.0;
		last_base = send_base;
		goodput = goodput / 2.0; // 2 seconds

		printf("[%3d] B %6d (%4.1f MB) N %5d T %d F %d W %d S %.3f Mbps RTT %.3f\n",
			(int)((clock() - constructor_start) / CLOCKS_PER_SEC), // Elapsed time in seconds
			send_base,
			sent_bytes / 1e6, // in MB
			next_seq,
			cnt_timeouts,
			num_fast_retrans, // number of fast retransmissions
			effective_window_size,
			goodput,
			(float)estimated_rtt / 1000.0
		);
	}

	return;
}


int SenderSocket::Send(char* buf, int bytes)
{
	if (sock == INVALID_SOCKET) {
		return NOT_CONNECTED;
	}
	//printf("Send: entered\n");
	HANDLE arr[] = { event_quit, empty };
	int ret = WaitForMultipleObjects(2, arr, false, INFINITE);
	if (ret == WAIT_OBJECT_0) {
		return last_status;
	}
	// no need for mutex as no shared variables are modified
	int slot = next_seq % W;
	Packet* p = pending_pkts + slot; // pointer to packet struct
	SenderDataHeader* sdh = (SenderDataHeader*)p->pkt;
	sdh->seq = next_seq;
	// set up remaining fields in sdh and p
	sdh->flags.reserved = 0;
	sdh->flags.magic = MAGIC_PROTOCOL;
	sdh->flags.ACK = 0;
	sdh->flags.SYN = 0;
	sdh->flags.FIN = 0;
	p->size = bytes + sizeof(SenderDataHeader);
	p->txTime = clock();
	// copy the data
	memcpy(sdh + 1, buf, bytes);
	next_seq++;
	ReleaseSemaphore(full, 1, NULL);



	return last_status;
}



void SenderSocket::WorkerRun() {
	// printf("WorkerRun: entered\n");

	// set the socket_receive_ready
	socket_receive_ready = CreateEvent(NULL, false, false, NULL);
	// associate the socket with the event
	if (WSAEventSelect(sock, socket_receive_ready, FD_READ) == SOCKET_ERROR) {
		// quit gracful with set event
		SetEvent(event_quit);
		printf("[%.3f] --> failed WSAEventSelect with %d\n",
			(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
		last_status = FAILED_SEND;
		return;
	}

	int kernelBuffer = 20e6; // 20 meg
	if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR) {
		// quit gracful with set event
		SetEvent(event_quit);
		printf("[%.3f] --> failed WSAEventSelect with %d\n",
			(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
		last_status = FAILED_SEND;
		return;
	}
	kernelBuffer = 20e6; // 20 meg
	if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (char*)&kernelBuffer, sizeof(int)) == SOCKET_ERROR) {
		// quit gracful with set event
		SetEvent(event_quit);
		printf("[%.3f] --> failed WSAEventSelect with %d\n",
			(float)(clock() - constructor_start) / CLOCKS_PER_SEC, WSAGetLastError());
		last_status = FAILED_SEND;
		return;
	}

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	HANDLE events[] = { socket_receive_ready, single_pkt_case, event_quit };
	// time the rtt of send_base
	clock_t sample_start = 0;
	int num_retries = 0;
	Packet* p;
	clock_t timer_expire = 0;
	bool reset = true;
	int dup_ack = 0;

	while (true) {
		// if pending packets
		// timeout = timerExpire - curTime
		// else timeout = INFINITE
		DWORD timeout = INFINITE;
		int ret;
		if (has_pending_pkts()) {
			clock_t cur_time = clock();
			timeout = timer_expire >= cur_time ? ((float)(timer_expire - cur_time) / CLOCKS_PER_SEC * 1000.0) : 0;
			ret = WaitForMultipleObjects(2, events, false, timeout);
		}
		else {
			ret = WaitForMultipleObjects(3, events, false, INFINITE);
			// printf("WorkerRun: no pending packets ret = %d\n", ret);
		}
		//printf("WorkerRun: ret = %d\n", ret);
		switch (ret) {
			// case timeout
		case WAIT_TIMEOUT: {
			//printf("[%.3f] --> timeout; rto = %d; send_base = %d; next_send_seq = %d\n",
			//	(float)(clock() - constructor_start) / CLOCKS_PER_SEC, rto, send_base, nxt_send_seq);
			cnt_timeouts++;

			if (num_retx >= MAX_ATTEMPTS) {
				// quitr graceful
				SetEvent(event_quit);
				last_status = TIMEOUT;
				return;
			}
			// retransmit base 
			//get the pkt to send 
			p = pending_pkts + send_base % W;
			reset = true;
			// printf("[%.3f] --> retransmitting packet %d\n",
			//	(float)(clock() - constructor_start) / CLOCKS_PER_SEC, send_base);
			// send p->pkt
			sample_start = clock();
			num_retx++;
			int s_res = sendto(sock, p->pkt, p->size, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
			if (s_res == SOCKET_ERROR) {

				SetEvent(event_quit);
				last_status = FAILED_SEND;
				return;
			}
			break;
		}
						 // case socket
		case WAIT_OBJECT_0: // socket event 
		{
			// printf("WorkerRun: socket event\n");
			// rece the packet
			ReceiverHeader rh;
			int addr_len = sizeof(target_addr);
			int bytes = recvfrom(sock, (char*)&rh, sizeof(ReceiverHeader), 0, (struct sockaddr*)&target_addr, &addr_len);

			if (bytes == SOCKET_ERROR) {
				// recv error 
				SetEvent(event_quit);
				last_status = FAILED_RECV;
				return;
			}

			// check if ack
			if (rh.flags.ACK) {
				// print ack
				//printf("[%.3f] <-- ACK %d; send_base = %d; next_send_seq = %d\n",
				//	(float)(clock() - constructor_start) / CLOCKS_PER_SEC, rh.ackSeq, send_base, nxt_send_seq);

				if (rh.ackSeq > send_base) {
					// reset dup ack
					dup_ack = 0;

					// update send_base since cumulative ackno
					send_base = rh.ackSeq;

					// update rt0
					if (num_retx == 0) {
						Packet* prev_pkt = pending_pkts + (send_base - 1) % W;
						sample_start = prev_pkt->txTime;

						calculate_rto(sample_start);
					}

					num_retx = 0;
					// if therer are currentlyh not-yet acked packets then restart the timer with latest RTO
					reset = true;

					// flow control
					effective_window_size = min(W, rh.recvWnd);
					// printf("effective_window_size = %d\n", rh.recvWnd);
					// how much we can advance the semephore
					int new_released = send_base + effective_window_size - last_released;

					ReleaseSemaphore(empty, new_released, NULL);
					last_released += new_released;
				}

				else if (rh.ackSeq == send_base) {
					// duplicate ack
					dup_ack++;
					if (dup_ack == 3) {
						// fast retransmit
						num_fast_retrans++;
						p = pending_pkts + send_base % W;

						sample_start = clock();
						reset = true;
						num_retx++;
						int s_res = sendto(sock, p->pkt, p->size, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
						if (s_res == SOCKET_ERROR) {
							SetEvent(event_quit);
							last_status = FAILED_SEND;
							return;
						}

						//dup_ack = 0;
					}
				}


			}
			break;
		}
		case WAIT_OBJECT_0 + 1:
			// do nothing 
			break;
			// case quit
		case WAIT_OBJECT_0 + 2:

			// printf("WorkerRun: exiting\n");
			return;
		default: // handle failed wait
			break;
		}
		// if send_base == nxt_send_seq or timeout or base move forward
		if (send_base == nxt_send_seq || reset) {
			// reset timer 
			// rto in ms

			timer_expire = clock() + (rto * CLOCKS_PER_SEC / 1000.0);
			reset = false;
		}
	}

	return;
}



void SenderSocket::Worker2Run() {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);

	HANDLE events[] = { full, event_quit };

	while (true) {

		int ret = WaitForMultipleObjects(2, events, false, INFINITE);

		if (ret == WAIT_OBJECT_0) {
			// get the packet to send
			Packet* p = pending_pkts + nxt_send_seq % W;

			// start the timer for rtt 
			clock_t sample_start = clock();
			p->txTime = clock();

			int s_res = sendto(sock, p->pkt, p->size, 0, (struct sockaddr*)&target_addr, sizeof(target_addr));
			if (s_res == SOCKET_ERROR) {
				SetEvent(event_quit); // quit gracful
				last_status = FAILED_SEND;
				return;
			}
			SetEvent(single_pkt_case);
			nxt_send_seq++;
		}
		else if (ret == WAIT_OBJECT_0 + 1) { // quit 
			return;
		}
	}
}