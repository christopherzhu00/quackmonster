#include "Header.h"
#include <string>
#include <iostream>

using namespace std;

Header::Header(bool ackFlag, bool synFlag, bool finFlag, uint16_t seqNum, uint16_t ackNum, uint16_t dSize) {
	if (ackFlag) {
		if (synFlag) {
			if (finFlag) {
				flagsNum = 7; //ack, syn, fin
			}

			else {
				flagsNum = 6; //ack, syn
			}
		}

		else {
			if (finFlag) {
				flagsNum = 5; //ack, fin
			}

			else {
				flagsNum = 4; //ack
			}
		}
	}

	else {
		if (synFlag) {
			if (finFlag) {
				flagsNum = 3; //syn, fin
			}

			else {
				flagsNum = 2; //syn
			}
		}

		else {
			if (finFlag) {
				flagsNum = 1; //fin
			}

			else {
				flagsNum = 0; //none
			}
		}
	}

	sequenceNumber = seqNum;
	ackNumber = ackNum;
	dataSize = dSize;
}

uint16_t Header::getSeqNum() {
	return sequenceNumber;
}

uint16_t Header::getAckNum() {
	return ackNumber;
}

uint16_t Header::getWindowSize() {
	return dataSize; 
}

uint16_t Header::getFlags() {
	return flagsNum;
}