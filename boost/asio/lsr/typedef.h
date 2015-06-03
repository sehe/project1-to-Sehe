/*
 * Author: solozobov
 * Created on 2 Март 2013 г., 14:10
 */
#ifndef TYPEDEF_H
#define	TYPEDEF_H

#include <unordered_set> 
#include <unordered_map>
//#include <types.h> // __uint32_t, __uint16_t, __uint8_t, __int32_t, int16_t, __int8_t
#include <netinet/in.h> // in_addr_t

typedef __uint8_t ui8;
typedef __uint16_t ui16;
typedef __uint32_t ui32;
typedef __int8_t si8;
typedef __int16_t si16;
typedef __int32_t si32;

typedef in_addr_t ip_address;

typedef std::unordered_set<int> IntSet;
typedef std::unordered_set<int>::iterator IntSetIterator;

typedef std::unordered_set<unsigned int> UIntSet;
typedef std::unordered_set<unsigned int>::iterator UIntSetIterator;

typedef std::unordered_map<int, int> IntIntMap;
typedef std::unordered_map<int, int>::iterator IntIntMapIterator;

typedef std::unordered_map<unsigned int, unsigned int> UIntUIntMap;
typedef std::unordered_map<unsigned int, unsigned int>::iterator UIntUIntMapIterator;

typedef std::unordered_multimap<unsigned int, unsigned int> UIntUIntMultimap;
typedef std::unordered_multimap<unsigned int, unsigned int>::iterator UIntUIntMultimapIterator;
typedef std::pair<unsigned int, unsigned int> UIntUIntMultimapEntry;

typedef std::unordered_multimap<int, int> IntIntMultimap;
typedef std::unordered_multimap<int, int>::iterator IntIntMultimapIterator;
typedef std::pair<int, int> IntIntMultimapEntry;

#endif	/* TYPEDEF_H */
