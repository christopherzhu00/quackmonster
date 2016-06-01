#ifndef HEADER_H
#define HEADER_H

#include <string>
#include <cstring>
#include "constants.h"

using namespace std;

struct Header {
	public:
		Header() = default;
		Header(bool ackFlag, bool synFlag, bool finFlag, uint16_t seqNum, uint16_t ackNum, uint16_t dSize);
		uint16_t getSeqNum();
		uint16_t getAckNum();
		uint16_t getWindowSize();
		uint16_t getFlags();
	private:
		uint16_t sequenceNumber; //2 bytes
		uint16_t ackNumber; //2 bytes
		uint16_t dataSize; //2 bytes
		uint16_t flagsNum; //2 bytes
} __attribute__((__packed__));

#endif