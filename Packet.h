#ifndef PACKET_H
#define PACKET_H

#include "Header.h"

using namespace std;

class Packet {
	public:
		Packet() = default;
		Packet(bool ackFlag, bool synFlag, bool finFlag, uint16_t seqNum, uint16_t ackNum, uint16_t dSize, char* data, int flag);
		~Packet();
		char* getData();
		Header getHeader();
		int getFlag();
		void setFlag();
		void unsetFlag();
		// uint16_t getDataSize();
	private:
		Header head;
		int m_flag;
		char data[BODY_SIZE];
};

#endif