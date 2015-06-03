/*
 * Author: solozobov
 * Created on 2 Март 2013 г., 13:48
 */

#ifndef DEBUG_H
#define	DEBUG_H

#include <stdio.h> // printf
#include "typedef.h" // ui32, ui16, ui8, si32m si16, si8, ip_address

namespace boost {
namespace asio {
namespace lsr {
namespace debug {
	static void bit(ui8 *ptr, ui32 length) {
		printf("%p\n", ptr);
		for (int j = 0; j < length; j++) {
			ui8 byte = ptr[j];
			for (int i = 7; i >= 0; i--) {
				if((byte>>i) % 2 == 0) {
					printf("0");
				} else {
					printf("1");
				}
			}

			if (j % 4 ==3) {
				printf("\n");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}

	static void printHexSymbol(ui8 hex) {
		switch (hex) {
			case 0: printf("0"); break;
			case 1: printf("1"); break;
			case 2: printf("2"); break;
			case 3: printf("3"); break;
			case 4: printf("4"); break;
			case 5: printf("5"); break;
			case 6: printf("6"); break;
			case 7: printf("7"); break;
			case 8: printf("8"); break;
			case 9: printf("9"); break;
			case 10: printf("A"); break;
			case 11: printf("B"); break;
			case 12: printf("C"); break;
			case 13: printf("D"); break;
			case 14: printf("E"); break;
			case 15: printf("F"); break;
			default: printf("-");
		}
	}

	static void hex(ui8 *ptr, ui32 length) {
		printf("%p\n", ptr);
		for (int j = 0; j < length; j++) {
			ui8 byte = ptr[j];
			printHexSymbol(byte >> 4);
			printHexSymbol(byte % 16);

			if (j % 4 ==3) {
				printf("\n");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
} // namespace debug
} // namespace lsr
} // namespace asio
} // namespace boost

#endif	/* DEBUG_H */
