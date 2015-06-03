/* 
 * File:   Net.h
 * Author: solozobov
 *
 * Created on 15 Апрель 2013 г., 0:00
 */

#ifndef TRACE_H
#define	TRACE_H

#define DEBUG

#include <sys/socket.h> // socket
#include <sys/time.h>   // gettimeofday, timeval
#include <netinet/in.h> // sockaddr_in
#include <unistd.h> // close, fork
#include <fcntl.h> // fcntl
#include <arpa/inet.h> // inet_addr, inet_ntoa

#include <boost/asio/lsr/debug.h>

namespace boost {
namespace asio {
namespace lsr {
namespace trace {
	
	using namespace std;
	
	const int TRACE_DEPTH = 40;
	const int MAX_NO_ANSWER = 4;
	const int SEND_RECEIVE_ATTEMPT_FAIL_SLEEP = 1000000;
	const int MAX_SEND_RECEIVE_ATTEMPTS_FAIL_NUM = 2;
	const int RECEIVE_ATTEMPT_FAIL_SLEEP = 200000;
	const int MAX_RECEIVE_ATTEMPTS_FAIL_NUM = 4;
	const int RESPONSE_BUFFER_LENGTH = 512;
	
	int createUdpSocket() {
		int result = socket(AF_INET, SOCK_RAW, IPPROTO_UDP);
		if (result < 0) {
			perror("Can't create socket UDP");
		}
		
		int on = 1;
		if (setsockopt(result, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
			perror("Can't set IP_HDRINCL option");
		}

/*		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = INADDR_ANY;
		addr.sin_port = 0;

		if (bind(result, (sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("Can't bind socket");
		}
*/
		return result;
	}

	int createIcmpSocket() {
		int result = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (result < 0) {
			perror("Can't create socket ICMP");
		}

		int flags = fcntl(result, F_GETFL, NULL);
		if (fcntl(result, F_SETFL, flags | O_NONBLOCK) < 0) {
			perror("Can't set O_NONBLOCK flag");
		}

		return result;
	}
	
	ui16 ipChecksum(ui8 *buffer, ui32 length) {
		ui32 result = 0;
		ui16 *buf = (ui16 *) buffer;
		
		while (length > 1) {
			result += *buf++;
			length -= 2;
		}
		if (length > 0) {
			result += * (unsigned char *) buf;
		}
		
		result = (result >> 16) + (result & 0xFFFF);
		result += (result >> 16);
		return ~((ui16) result);
	}
	
	void sendRequest(int socketDescriptor, ui32 ip, ui16 port, ui8 ttl,
	                 unsigned int* route, int route_length)
	{
		sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = ip;

		ui8 header_length = 5;
		ui8 buffer_length = 36;
		if (route_length > 0) {
			header_length += 1 + route_length;
			buffer_length += 4 * (route_length + 1);
		}
		ui8 buffer[buffer_length];
		buffer[0] =  64 + header_length; buffer[1] =  0x00; buffer[2]  =  0x00;  buffer[3] =  buffer_length;
		buffer[4] =  0x00;               buffer[5] =  0x00; buffer[6]  =  0x00;  buffer[7] =  0x00;
		buffer[8] =  ttl;                buffer[9] =  0x11; buffer[10] =  0x00; buffer[11] =  0x00;
		((ui32*) buffer)[3] = 0;
		if (route_length > 0) {
			((ui32*) buffer)[4] = route[0];
		} else {
			((ui32*) buffer)[4] = ip;
		}
		int i = 20;
		
		if (route_length > 0) {
			buffer[i++] = 1; buffer[i++] = 131; buffer[i++] = (route_length + 1) * 4 -1; buffer[i++] = 4;
			memcpy(buffer + i, route + 4, (route_length - 1) * 4);
			i += route_length * 4;
			((ui32*) (buffer + i - 4))[0] = ip;
		}
		
		((ui16*) (buffer + i))[0] = htons(port); ((ui16*) (buffer + i))[1] = htons(port);
		((ui16*) (buffer + i))[2] = htons(16);   ((ui16*) (buffer + i))[3] = ipChecksum(buffer + i, 16);
		((ui16*) (buffer + i))[4] = 1;           ((ui16*) (buffer + i))[5] = 2;
		((ui16*) (buffer + i))[6] = 3;           ((ui16*) (buffer + i))[7] = 4;
		debug::hex(buffer, buffer_length);
		
		if (sendto(socketDescriptor, buffer, buffer_length, 0, (sockaddr *) &addr, sizeof(addr)) < 0) {
			perror("Can't send");
		}
	}

	int receiveAnswer(int socketDescriptor, char *buffer, int bufferLength) {
		sockaddr_in addr;
		int addrLen = sizeof(addr);

		usleep(100000);
		for (int attempt = 0; attempt < MAX_RECEIVE_ATTEMPTS_FAIL_NUM; attempt++) {
			int result = recvfrom(socketDescriptor, buffer, bufferLength, 0, (sockaddr *) &addr, (socklen_t *)&addrLen);
			if (result > 0) {
#ifdef DEBUG
				printf("Answer from %s (%d attempt)\n", inet_ntoa(addr.sin_addr), attempt + 1);
#endif
				return result;
			} else if (errno != EWOULDBLOCK) {
				perror("Unknown error during recvfrom()");
				return -1;
			}
			usleep(RECEIVE_ATTEMPT_FAIL_SLEEP);
		}

#ifdef DEBUG
		printf("No answer in %d milliseconds\n", MAX_RECEIVE_ATTEMPTS_FAIL_NUM * RECEIVE_ATTEMPT_FAIL_SLEEP);
#endif
		return -1;
	}
	

	int sendAndReceive(unsigned int ip, int port, int ttl, unsigned int* route, int routelength, int sendSocket, int receiveSocket, char* buffer, int bufferLength) {
		int result = -1;
		for (int attempt = 0; attempt < MAX_SEND_RECEIVE_ATTEMPTS_FAIL_NUM; attempt++, port++) {
			sendRequest(sendSocket, ip, port, ttl, route, routelength);
#ifdef DEBUG
				printf("Sent to port %d with TTL %d (attempt %d)\n", port, ttl, attempt + 1);
#endif
			result = receiveAnswer(receiveSocket, buffer, bufferLength);
			if (result > 0) {
				return result;
			}
			usleep(SEND_RECEIVE_ATTEMPT_FAIL_SLEEP);
		}
		return -1;
	}

	int getIpDataOffset(char* buffer, int bufferLength) {
		if (bufferLength >= 20 && (buffer[0] & 0xF0) == 64 && buffer[9] == 1) {
			int result = (buffer[0] & 0x0F) * 4;
			if (result >= 20 && result < bufferLength) {
				return result;
			} else {
				printf("Invalid IP packet with dataOffset = %d:\n", result);
				debug::hex((ui8 *) buffer, bufferLength);
			}
		} else {
			printf("IP packet not detected:\n");
			debug::hex((ui8 *) buffer, bufferLength);
		}
		return -1;
	}

	bool isDestinationUnreachable(char * buffer, int bufferLength) {
		int dataOffset = getIpDataOffset(buffer, bufferLength);
		if (dataOffset < 0) {
			return false;
		}

		return buffer[dataOffset] == 3;
	}

	bool isTimeToLiveExceded(char * buffer, int bufferLength) {
		int dataOffset = getIpDataOffset(buffer, bufferLength);
		if (dataOffset < 0) {
			return false;
		}

		return buffer[dataOffset] == 11 && buffer[dataOffset + 1] == 0;
	}

	void traceRoute(unsigned int ip, unsigned int *lsr, int lsrLength, int sendSocket, int receiveSocket, vector<unsigned int>& route) {
#ifdef DEBUG
		sockaddr_in address;
		address.sin_addr.s_addr = ip;
		std::cout << ip << std::endl;
		printf("Tracing route to %s\n", inet_ntoa(address.sin_addr));
#endif
		route.clear();
		char buffer[RESPONSE_BUFFER_LENGTH];

		int noAnswerCount = 0;
		int port = 30005;
		int ttl;
		for (ttl = 1; ttl <= TRACE_DEPTH; ttl++, port++) {
			int answerLength = sendAndReceive(ip, port, ttl, lsr, lsrLength, sendSocket, receiveSocket, buffer, RESPONSE_BUFFER_LENGTH);

			if (answerLength < 0) {
				noAnswerCount++;
				if (noAnswerCount == MAX_NO_ANSWER) {
					route.resize(ttl - noAnswerCount);
					return;
				} else {
					route.push_back(0);
				}
			} else {
				if (isDestinationUnreachable(buffer, answerLength)) {
					route.push_back(((ui32 *) buffer)[3]);
					return;
				} else if (isTimeToLiveExceded(buffer, answerLength)) {
					unsigned int answerIp = ((ui32 *) buffer)[3];
					route.push_back(answerIp);
					if (answerIp == ip) {
						return;
					}
					noAnswerCount = 0;
				} else {
					printf("Unknown packet format:");
					debug::hex((ui8 *) buffer, answerLength);
					route.resize(ttl - 1 - noAnswerCount);
					return;
				}
			}
		}
	}
} // namespace trace
} // namespace lsr
} // namespace asio
} // namespace boost

#endif	/* TRACE_H */

