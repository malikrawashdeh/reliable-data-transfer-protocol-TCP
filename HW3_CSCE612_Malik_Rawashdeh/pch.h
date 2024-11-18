// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H
#define PCH_H

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")

// add headers that you want to pre-compile here

#include <stdio.h> // printf etc
#include <chrono>  // for clokc
#include <thread>
// min 
#include <algorithm>
#include <WinSock2.h>
#include <windows.h>

using std::thread;

// cpp files i created
#include "Checksum.h"
#include "PacketHeaders.h"
#include "SenderSocket.h"

#endif //PCH_H
