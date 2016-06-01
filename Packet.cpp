#include "Packet.h"
#include <iostream>

using namespace std;

Packet::Packet(bool ackFlag, bool synFlag, bool finFlag, uint16_t seqNum, uint16_t ackNum, uint16_t dSize, char* d, int flag) : head(ackFlag, synFlag, finFlag, seqNum, ackNum, dSize) {
	strncpy(data, d, dSize);
	m_flag = flag;
}

Packet::~Packet() {

}

char* Packet::getData() {
	return data;
}

Header Packet::getHeader() {
	return head;
}

void Packet::setFlag()
{
	m_flag = 1;
}

void Packet::unsetFlag()
{
	m_flag = 0;
}

int Packet::getFlag()
{
	return m_flag;
}
// uint16_t Packet::getDataSize() {
// 	return head.getDataSize();;
// }