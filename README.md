# Reliable Data Transfer Protocol - TCP Implementation

This project implements a reliable data transfer (RDT) protocol using the principles of Transmission Control Protocol (TCP). It is designed as a final completed project, integrating all features necessary for reliable and efficient data transfer over an unreliable UDP channel.

## Features

- Implements TCP-like reliability over UDP, including connection setup, reliable data transfer, and graceful termination.
- Supports high-throughput scenarios, capable of achieving hundreds of Mbps under varying network conditions.
- Integrates dynamic retransmission timeouts (RTO) and cumulative ACKs with pipelining for enhanced performance.
- Includes advanced TCP features such as fast retransmission, flow control, and handling of heavy packet loss.

## Program Details

The core of the project revolves around the `SenderSocket` class, which provides the following key functionalities:

- **Open**: Establishes a connection using a TCP-like handshake (SYN and SYN-ACK) with configurable network parameters.
- **Send**: Reliably sends data using pipelined packet transmission with cumulative ACKs.
- **Close**: Terminates the connection gracefully using a FIN/FIN-ACK handshake.

The implementation also features two additional threads:
1. **Worker Thread**: Handles the transmission and retransmission of packets, as well as ACK processing.
2. **Stats Thread**: Periodically prints real-time performance metrics, including throughput, RTT, and retransmission counts.

### Supported Features
- **Connection Management**:
  - Implements TCP-like SYN and FIN handshakes for connection setup and termination.
  - Dynamically adjusts RTO values using TCP estimation algorithms.
- **Reliable Data Transfer**:
  - Uses 4-byte sequence numbers for packet identification.
  - Computes CRC-32 checksums to verify data integrity.
  - Handles packet loss with retransmissions triggered by timeouts or duplicate ACKs.
- **Flow Control and Congestion Management**:
  - Adapts the sender's window size based on receiver feedback.
  - Supports pipelined transmission for high throughput.

## Usage

The program can be executed from the command line with the following syntax:

```bash
./rdt <destination_server> <buffer_size> <sender_window> <RTT> <loss_forward> <loss_reverse> <link_speed>
```
## Parameters

- **destination_server**: Hostname or IP address of the receiver.
- **buffer_size**: Size of the transmission buffer, specified as a power of 2.
- **sender_window**: Sender's sliding window size in packets.
- **RTT**: Round-trip propagation delay in seconds.
- **loss_forward**: Probability of packet loss in the forward direction.
- **loss_reverse**: Probability of packet loss in the reverse direction.
- **link_speed**: Bottleneck link speed in Mbps.

## Example Output

```plaintext
Main: sender W = 5000, RTT 0.200 sec, loss 0.001 / 0.0001, link 100 Mbps
[ 2] B 184 (0.3 MB) N 368 T 0 F 1 W 184 S 1.077 Mbps RTT 0.219
[ 4] B 2821 (4.2 MB) N 5642 T 1 F 5 W 2821 S 15.408 Mbps RTT 0.218
Main: transfer finished in 28.626 sec, 37509.93 Kbps, checksum FC6FB7CB
```
## Technical Highlights

- **Programming Language**: C++
- **Network Protocol**: UDP with custom reliable transport-layer implementation.
- **Platform**: Tested on Windows using Winsock for socket programming.

## How to Build and Run

1. Clone the repository:
   ```bash
   git clone https://github.com/malikrawashdeh/reliable-data-transfer-protocol-TCP/
   cd reliable-data-transfer-protocol-TCP
```
1. Build the project using Visual Studio for best experience

## Performance Metrics

The program includes a stats thread that provides periodic performance updates:

- **B**: Base sequence number of the sender's sliding window.
- **N**: Next sequence number to be sent.
- **T**: Number of packets retransmitted due to timeouts.
- **F**: Number of packets fast-retransmitted.
- **W**: Effective sender window size.
- **S**: Goodput (Mbps).
- **RTT**: Estimated round-trip time (seconds).

