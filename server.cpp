#include <string>
#include <thread>
#include <iostream>
#include <sys/socket.h> //sockets
#include <string>
#include <stdlib.h>
#include <time.h>
#include "Header.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "Packet.h"
#include <fstream>
#include <algorithm>
#include <list>
#include <unordered_map>
#include <sys/time.h>

using namespace std;

// state constants
const int SYN_ACK_SENT = 200;
const int NORMAL = 220;
const int CLOSING = 300;

int main(int argc, const char* argv[]) {
	// wrong number of arguments, throw error
	if (argc != 3) {
		cerr << "Not correct number of arguments" << endl;
		exit(3);
	}

	

	int client_sock;
	
	
	
	srand(time(NULL));

	string filePath = argv[2];
	struct sockaddr_in clientaddr;
	socklen_t clientlen = sizeof(clientaddr);

	// new datagram socket for server
	int fd = socket(AF_INET, SOCK_DGRAM, 0);

	// couldn't initialize socket, give error
	if (fd < 0) {
		cerr << "Error with socket" << endl;
		exit(5);
	}

	int state;

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 500000;

	// allows socket to be reused quickly?
	int status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	if (status < 0) {
		cerr << "Error with setsockopt" << endl;
		exit(6);
	}

	struct sockaddr_in serv_addr;

	// initilaize struct to all 0s
	bzero((char *) &serv_addr, sizeof(serv_addr));
	
	// server address specifics
	serv_addr.sin_family = AF_INET;
	
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(atoi(argv[1]));
	memset(serv_addr.sin_zero, 0, 8);
	
	// couldn't bind socket, give error
	if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
		cerr << "Error while binding socket" << endl;
		exit(18);
	}

	// buffer to store incoming packet
	// char buf[1032];

	Packet packet;

	for (;;) {
		status = recvfrom(fd, (void*) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, &clientlen);
		
		if (status < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				cerr << "testing" << endl;
				continue;
			}

			else {
				cerr << "Error with receiving SYN from client" << endl;
				exit(123);
			}
		}

		else if (status == 0) {
			cerr << "Did not receive any data" << endl;
			break;
		}

		else {
			if (packet.getHeader().getFlags() == 2) {
				break;
			}
		}
	}

	// cast the buffer into a new packet variable 
	// Packet* syn = reinterpret_cast<Packet*>(buf);

	// packet's header
	Header header = packet.getHeader();

	// packet's flags
	uint16_t flags = header.getFlags();

	// sequence number will have to wrap over
	uint16_t sequenceNumber = rand() % MAX_SEQUENCE_NUMBER;

	// define new packet for the SYN-ACK to be sent
	Packet synAck(true, true, false, sequenceNumber, header.getSeqNum()+1, 0, nullptr, 0);

	struct hostent *hostp;

	// get client's address
	hostp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr, sizeof(clientaddr.sin_addr.s_addr), AF_INET);
	
	char *hostaddrp = inet_ntoa(clientaddr.sin_addr);
	if (!hostaddrp) {
		cerr << "Error with inet_ntoa" << endl;
		exit(22);
	}

	// send SYN-ACK to client
	if (sendto(fd, (void *) &synAck, sizeof(synAck), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
		cerr << "Error sending SYN-ACK to client" << endl;
		exit(32);
	}

	uint16_t seqNumber = (synAck.getHeader().getSeqNum() + 1) % MAX_SEQUENCE_NUMBER;
	uint16_t baseNumber = seqNumber;

	uint16_t ackNumber;
	uint16_t pseudo_ackNumber;				// chris code
	uint16_t init_ackNumber;
	uint16_t init_seqNumber;

	// set server state to show SYN-ACK has been sent
	state = SYN_ACK_SENT;

	for (;;) {
		status = recvfrom(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, &clientlen);
		if (status < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				int timeoutStatus = sendto(fd, (void *) &synAck, sizeof(synAck), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr));
				if (timeoutStatus < 0) {
					cerr << "Error re-sending SYN-ACK" << endl;
					exit(124);
				}
			}

			else {
				cerr << "Error receiving ACK from server" << endl;
				exit(84);
			}
		}

		else if (status == 0) {
			cout << "Did not receive any data" << endl;
			break;
		}

		else {
			if (packet.getHeader().getFlags() == 4) {
				ackNumber = (packet.getHeader().getSeqNum() + 1) % MAX_SEQUENCE_NUMBER;
				break;
			}

			else
				cerr << "Received improper packet (Not SYN-ACK)";
		}
	}
	
	int recvWindowSize = 1024;

	std::ifstream file(filePath, std::ifstream::binary);

	int slowStartThreshold = 30720;
	
	struct timeval p1; 		// going to use this as timeout holder for starting
	struct timeval p2;		// timeout for after
	int index;			// used for for loops
	long elapse;
	char* init_data;		// used to placeholder for first packet
	int init_sentDataSize;
	int timeout_flag = 0;
	int init_remaining;
	int init_filesize;
	int init_fileindex;
	
	if (file.is_open()) {
		file.seekg (0, file.end);
		int filesize = file.tellg();
		file.seekg(0, file.beg);
		int fileIndex = 0;

		//cout << "filesize is: " << filesize << " bytes" << endl;

		int congestion_window = 1024;

		const int SAFE_SEND_SIZE = (MAX_SEQUENCE_NUMBER+1)/2;
		/*
		
		while (fileIndex < filesize) {
			int sendWindowIndex = 0;
			int sendWindowLength = min(congestion_window, min(recvWindowSize, SAFE_SEND_SIZE)); 
			while (sendWindowIndex < sendWindowLength && fileIndex < filesize) {
				char* data;
				int remaining = filesize - fileIndex;
				if (remaining >= BODY_SIZE) {
					data = new char[BODY_SIZE];
					file.read(data, BODY_SIZE);
					fileIndex += BODY_SIZE;
				}

				else {
					data = new char[remaining];
					file.read(data, remaining);
					fileIndex += remaining;
				}

				int sentDataSize = min(BODY_SIZE, remaining);

				packet = Packet(0, 0, 0, sequenceNumber, ackNumber, sentDataSize, data);

				// char sendBuffer[PACKET_LENGTH];
				// memcpy(sendBuffer, &pack, PACKET_LENGTH);
				
				//Packet* reconstructed = reinterpret_cast<Packet*>(sendBuffer);
				//cout << reconstructed->getData() << endl; //has data here

				cout << "Sending data packet " << sequenceNumber << " " << congestion_window/BODY_SIZE << " " << slowStartThreshold/BODY_SIZE << endl;

				if (sendto(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
					cerr << "Server couldn't send packet.\n" << endl;
					exit(45);
				}

				Packet ack;

				if (recvfrom(fd, (void*) &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr, &clientlen) < 0) {
					if (errno == EWOULDBLOCK || errno == EAGAIN) {
						cerr <<"fuck" << endl;
						cerr << "Timeout" << endl;
						exit(82);
					}

					else {
						cerr << "recvfrom failed" << endl;
						exit(81);
					}
				}

				ackNumber = ack.getHeader().getAckNum();
				ackNumber %= MAX_SEQUENCE_NUMBER;

				cout << "Receiving ACK packet " << ackNumber << endl;

				sendWindowIndex += sentDataSize;
				sequenceNumber += sentDataSize;

				sequenceNumber %= MAX_SEQUENCE_NUMBER;

				delete [] data;
			}

			if (congestion_window < slowStartThreshold) {
				congestion_window *= 2;
			}

			else {
				congestion_window += BODY_SIZE;
			}

		
		*/
		
		
		
		
		
		
		
		//send actual file
		while (fileIndex < filesize) {
			int sendWindowIndex = 0;
			int sendWindowLength = min(congestion_window, min(recvWindowSize, SAFE_SEND_SIZE)); 
			while (sendWindowIndex < sendWindowLength && fileIndex < filesize) {
				char* data;
				int remaining = filesize - fileIndex;
			//	init_remaining = remaining;
				init_fileindex = fileIndex;
				init_filesize = filesize;
				if (remaining >= BODY_SIZE) {
					data = new char[BODY_SIZE];
					file.read(data, BODY_SIZE);
					fileIndex += BODY_SIZE;
				}

				else {
					data = new char[remaining];
					file.read(data, remaining);
					fileIndex += remaining;
				}
				init_data = data;
				int sentDataSize = min(BODY_SIZE, remaining);
				init_sentDataSize = sentDataSize;
				
				packet = Packet(0, 0, 0, sequenceNumber, ackNumber, sentDataSize, data, timeout_flag);
				init_ackNumber = ackNumber;
				init_seqNumber = sequenceNumber;
				// char sendBuffer[PACKET_LENGTH];
				// memcpy(sendBuffer, &pack, PACKET_LENGTH);
				
				//Packet* reconstructed = reinterpret_cast<Packet*>(sendBuffer);
				//cout << reconstructed->getData() << endl; //has data here
				if(timeout_flag == 0)
				{
				cout << "Sending data packet " << sequenceNumber << " " << congestion_window/BODY_SIZE << " " << slowStartThreshold/BODY_SIZE << endl;
				}
				else if(timeout_flag)
				{
					cout << "Sending data packet " << sequenceNumber << " " << congestion_window/BODY_SIZE << " " << slowStartThreshold/BODY_SIZE << " "<< "Retransmission" << endl;
				//	timeout_flag = 0;
				}
				
				gettimeofday(&p1, NULL);		// start timer right before we start sending
				for(index = 0; index < congestion_window/BODY_SIZE; index++)
				{
					if (sendto(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
						cerr << "Server couldn't send packet.\n" << endl;		
						index--;										// try resending   		this is essentially a timeout so might have to change
						continue;
						exit(45);
					}
					timeout_flag = 0;
					remaining = filesize - fileIndex;
					if (remaining >= BODY_SIZE) {
						data = new char[BODY_SIZE];
						file.read(data, BODY_SIZE);
						fileIndex += BODY_SIZE;
					}

					else {
						data = new char[remaining];
						file.read(data, remaining);
						fileIndex += remaining;
					}
					sentDataSize = min(BODY_SIZE, remaining);
					packet = Packet(0, 0, 0, sequenceNumber, ackNumber, sentDataSize, data, timeout_flag);
				}
				
				

				Packet ack;
				
				for(index = 0; index < congestion_window/BODY_SIZE; index++)
				{
					gettimeofday(&p2, NULL);
					elapse = (p2.tv_sec-p1.tv_sec)*1000000 + (p2.tv_usec - p1.tv_usec);		// = check within each iteration of receiving ack packets for timeout
					
					if(elapse > tv.tv_usec)		// packet loss occurred
					{
						slowStartThreshold = .5 * congestion_window;
						congestion_window = 1024;
						timeout_flag = 1;
						data = init_data;
						sequenceNumber = init_seqNumber;
						ackNumber = init_ackNumber;
						sentDataSize = init_sentDataSize;
					//	remaining = init_remaining;
						filesize = init_filesize;
						fileIndex = init_fileindex;
						break;				
					}
					if (recvfrom(fd, (void*) &ack, sizeof(ack), 0, (struct sockaddr*)&clientaddr, &clientlen) < 0) {
						if (errno == EWOULDBLOCK || errno == EAGAIN) {
							cerr << "fuck" << endl;
							cerr << "Timeout" << endl;
							slowStartThreshold = .5 * congestion_window;
						congestion_window = 1024;
						timeout_flag = 1;
						data = init_data;
						sequenceNumber = init_seqNumber;
						ackNumber = init_ackNumber;
						sentDataSize = init_sentDataSize;
					//	remaining = init_remaining;
						timeout_flag = 1;
						filesize = init_filesize;
						fileIndex = init_fileindex;
						cerr << "i break your skull" << endl;
						break;				
						//	exit(82);
						}

						else {
							cerr << "recvfrom failed" << endl;
							exit(81);
						}
					}
					pseudo_ackNumber = ack.getHeader().getAckNum();		// testing before finalizing in case of timeout
					pseudo_ackNumber %= MAX_SEQUENCE_NUMBER;
				}
				cerr << "negroes" << endl;
				if(timeout_flag)
				{
					cerr << "skull has been broken" << endl;
		//			timeout_flag = 0;			// essentially goes back in time to reset the stuff
					continue;
				}
				ackNumber = ack.getHeader().getAckNum();
				ackNumber %= MAX_SEQUENCE_NUMBER;

				cout << "Receiving ACK packet " << ackNumber << endl;

				sendWindowIndex += sentDataSize;
				sequenceNumber += sentDataSize;

				sequenceNumber %= MAX_SEQUENCE_NUMBER;

				delete [] data;
			}

			if (congestion_window < slowStartThreshold) {
				congestion_window *= 2;
			}
			
			else {
				congestion_window += BODY_SIZE;			// is this 1 for congestion avoidance? i think so
			}

			/*

			if (recvfrom(fd, (void*) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, &clientlen) < 0) {
				cout << "recvfrom failed" << endl;
				exit(81);
			}

			header = packet.getHeader();

			ackNumber += header.getAckNum();

			ackNumber %= MAX_SEQUENCE_NUMBER;

			cout << "Receiving ACK packet " << packet.getHeader().getAckNum() << "\n";

			*/
			// ack = reinterpret_cast<Header*>(buf);
			// cout << ack->getAckNum() << endl;
		}
	}

	else {
		cerr << "Couldn't find/open file" << endl;
		exit(99);
	}

	//initiate shutdown
	Packet fin(0, 0, 1, sequenceNumber, ackNumber, 0, nullptr, 0);

	// memcpy(buf, &fin, 8);
	for(;;)
	{
		if (sendto(fd, (void* ) &fin, sizeof(fin), 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
			cerr << "Error sending fin to client" << endl;
			cerr << "skull broken part2" << endl;
			continue;
		}
	//	exit(32);
	

	sequenceNumber += 1;

	sequenceNumber %= MAX_SEQUENCE_NUMBER;
	
	
	
		if (recvfrom(fd, (void*) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, &clientlen) < 0) {
			cerr << "Error receiving fin-ack from client" << endl;
			cerr << "fuck my skull hurts" << endl;
			sequenceNumber--;
			continue;
		//	exit(8);
		}
		else
		{
			break;
		}
	
	}
	ackNumber = (packet.getHeader().getAckNum() + 1) % MAX_SEQUENCE_NUMBER;

	// Client's FIN and FIN-ACK down here, will fix later

	// status = recvfrom(fd, (void*) &packet, sizeof(packet), 0, (struct sockaddr*)&clientaddr, &clientlen);
	// if (status < 0) {
	// 	cerr << "Error receiving fin from client" << endl;
	// 	exit(8);
	// }

	// Header* clientFin = reinterpret_cast<Header*>(buf);

	// Packet fAck(0, 1, 1, sequenceNumber, ackNumber, 0, "");
	// memcpy(buf, &fAck, 8);
	// if (sendto(fd, buf, 8, 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
	// 	cerr << "Error sending fin-ack to client" << endl;
	// 	exit(32);
	// }

	// usleep(500000);
	cerr << "good job chris" << endl	;
	return 0;
}


	// cout << "data to be received is: " << ack->getDataSize() << endl;
	//connection established



	// uint16_t recvWindowSize = ack->getDataSize(); //1024
	// sequenceNumber = ack->getSeqNum();
	// uint16_t ackNumber = ack->getAckNum();

 //    std::ifstream file(filePath, std::ifstream::binary);

	// if (file.is_open()) {
	// 	file.seekg (0, file.end);
	// 	int filesize = file.tellg();
	// 	file.seekg(0, file.beg);
	// 	int fileIndex = 0;

	// 	cout << "filesize is: " << filesize << " bytes" << endl;

	// 	uint16_t congestion_window = 1024;

	// 	//send actual file
	// 	while (fileIndex < filesize) {
	// 		char* data = new char[congestion_window]; //1024

	// 		uint16_t remaining = filesize - fileIndex;
	// 		if (remaining >= congestion_window) {
	// 			file.read(data, congestion_window);
	// 			fileIndex += congestion_window;
	// 		}

	// 		else {
	// 			file.read(data, remaining);
	// 			fileIndex += remaining;
	// 		}

	// 		//cout << data << endl; //data has html file

	// 		uint16_t dataSendLength = min(congestion_window, recvWindowSize); //1024

	// 		Packet pack(false, false, false, sequenceNumber, ackNumber, dataSendLength, data);

	// 		char sendBuffer[PACKET_LENGTH];
	// 		memcpy(sendBuffer, &pack, PACKET_LENGTH);
			
	// 		Packet* reconstructed = reinterpret_cast<Packet*>(sendBuffer);
	// 		cout << reconstructed->getData() << endl; //has data here

	// 		if (sendto(fd, sendBuffer, PACKET_LENGTH, 0, (struct sockaddr*)&clientaddr, sizeof(clientaddr)) < 0) {
	// 			cout << "sendto failed" << endl;
	// 			exit(45);
	// 		}

			

	// 		if (recvfrom(fd, (void*)buf, 8, 0, (struct sockaddr*)&clientaddr, &clientlen) < 0) {
	// 			cout << "recvfrom failed" << endl;
	// 			exit(81);
	// 		}



	// 		ack = reinterpret_cast<Header*>(buf);
	// 		cout << ack->getAckNum() << endl;


	// 		delete [] data;
	// 	}
	// }



	// else {
	// 	cerr << "Couldn't find/open file" << endl;
	// 	exit(99);
	// }




	// //initiate shutdown


	// Header fin(false, false, true, 1000, 2000, 0);
	// buf[8];