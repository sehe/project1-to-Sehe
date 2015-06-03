/* 
 * File:   IpAsnIndex.h
 * Author: solozobov
 *
 * Created on 6 Апрель 2013 г., 17:36
 */

#ifndef ASN_INFO_H
#define	ASN_INFO_H

#include <fstream>
#include <arpa/inet.h> // inet_addr, inet_ntoa

#include "typedef.h"

//#define DEBUG

namespace boost {
namespace asio {
namespace lsr {
namespace asn {

	struct AsnInfo {
		ui32 end_ip;
		ui32 start_ip;
		ui32 asn;
		AsnInfo *next;
	};

	const int ASN_ENTRIES_MAX_NUM = 200000;
	char const*IP_ASN_FILE_NAME = "ip2asn.tsv";

	ui32 ip2infoIndex[1024];

	typedef std::unordered_map<int, AsnInfo*> IntAsnInfoMap;
	typedef std::unordered_map<int, AsnInfo*>::iterator IntAsnInfoMapIterator;
	IntAsnInfoMap asn2info;

	AsnInfo asnInfo[ASN_ENTRIES_MAX_NUM];
	int asnInfoLength;

	void readIpAsnBase() {
		printf("Reading ASN IP base ...\n");
		std::ifstream in;
		in.open(IP_ASN_FILE_NAME);
		if (!in.is_open()) {
			printf("Can't open IP<->ASN info file '%s'\n", IP_ASN_FILE_NAME);
			exit(-1);
		}

		int i = 0;
		while (!in.eof()) {
			in >> asnInfo[i].start_ip >> asnInfo[i].end_ip >> asnInfo[i].asn;
			asnInfo[i].next = NULL;

			IntAsnInfoMapIterator iterator = asn2info.find(asnInfo[i].asn);
			if (iterator == asn2info.end()) {
				asn2info[asnInfo[i].asn] = &(asnInfo[i]);
			} else {
				AsnInfo *current = iterator->second;
				while (current->next != NULL) {
					current = current->next;
				}
				current->next = &(asnInfo[i]);
			}

			i++;
		}
		asnInfoLength = i;

		in.close();
		printf("Finished reading ASN IP base\n");
	}

	void buildIndex() {
		printf("Building ASN IP index\n");
		for (int j = 0, k = 0; j < 1024; j++) {
			ui32 ip = j << 22;
			while (k < asnInfoLength && asnInfo[k].end_ip < ip) {
				k++;
			}
			ip2infoIndex[j] = k;
		}
		printf("Finished building ASN IP index\n");
	}

	void init() {
		readIpAsnBase();
		buildIndex();
	}

	// @return -1 if ASN is unknown
	int getAsnInfo(ui32 ip) {
		int key = ip >> 22;
		int index = ip2infoIndex[key];

		while (index < asnInfoLength) {
			if (ip <= asnInfo[index].end_ip) {
				if (ip >= asnInfo[index].start_ip) {
					#ifdef DEBUG
						sockaddr_in addr;
						addr.sin_addr.s_addr = htonl(ip);
						printf("For ip %s (%u) detected ASN %d with range [%u, %u]\n", inet_ntoa(addr.sin_addr), ip, asnInfo[index].asn, asnInfo[index].start_ip, asnInfo[index].end_ip);
					#endif
					return index;
				} else {
					#ifdef DEBUG
						sockaddr_in addr;
						addr.sin_addr.s_addr = htonl(ip);
						printf("For ip %s (%u) no ASN detected\n", inet_ntoa(addr.sin_addr), ip);
					#endif
					return -1;
				}
			} else {
				index++;
			}
		}
		#ifdef DEBUG
			sockaddr_in addr;
			addr.sin_addr.s_addr = htonl(ip);
			printf("For ip %s (%u) no ASN detected\n", inet_ntoa(addr.sin_addr), ip);
		#endif
		return 0;
	}

	#ifdef DEBUG
		void diagnostics() {
			for (int i = 0; i < 1023; i++) {
				printf("%d\n", ip2infoIndex[i+1] - ip2infoIndex[i]);
			}
		}
	#endif
} // namespace asn
} // namespace lsr
} // namespace asio
} // namespace boost

#endif	/* ASNI_NFO_H */
