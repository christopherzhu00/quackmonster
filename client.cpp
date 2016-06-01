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
#include <list>
#include <sys/epoll.h>

using namespace std;

const int SYN_SENT = 100;

int main(int argc, const char* argv[]) {

	int state;

	if (argc != 3) {
		cerr << "Incorrect number of arguments" << endl;
		exit(3);
	}


	// struct addrinfo hints;
 //    struct addrinfo *res;

 //    // set up hints addrinfo
 //    memset(&hints, 0, sizeof(hints));
 //    hints.ai_family = AF_INET;
 //    hints.ai_socktype = SOCK_DGRAM;
 //    hints.ai_flags = AI_PASSIVE;

 //    //set up socket calls
 //    int status = getaddrinfo(argv[1], argv[2], &hints, &res);
 //    if (status != 0) {
 //        cerr << "getaddrinfo error" << endl;
 //        exit(1);
 //    }

 //    // find socket to connect to
 //    int fd;
 //    int yes = 1;
 //    auto i = res;
 //    for (; i != NULL; i = i ->ai_next)
 //    {
 //        fd = socket(res->ai_family, res->ai_socktype, 0);
 //        if (fd == -1) {
 //            continue;
 //        }

 //        status = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
 //        if (status == -1) {
 //            continue;
 //        }

 //        status = connect(fd, res->ai_addr, res->ai_addrlen);
 //        if (status == -1) {
 //            continue;
 //        }

 //        break;
 //    }
 //    freeaddrinfo(res);

 //    // check if reached end of linked list
 //    // means could not find a socket to bind to
 //    if (i == NULL) {
 //        perror("bind to a socket");
 //        exit(1);
 //    }



	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd < 0) {
		cerr << "Error with socket" << endl;
		exit(24);
	}

	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = 0;

	// allows socket to be reused quickly?
	int status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));     // im trying no timeout for client
//	int status = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, 0, 0);
	if (status < 0) {
		cerr << "Error with setsockopt" << endl;
		exit(6);
	}

	string host = argv[1];

	struct hostent *server;
	server = gethostbyname(host.c_str());

	struct sockaddr_in servAddr;
	bzero((char *) &servAddr, sizeof(servAddr));
	servAddr.sin_family = AF_INET;
	servAddr.sin_port = htons(atoi(argv[2]));	
	servAddr.sin_addr.s_addr = inet_addr(argv[1]);
	memset(servAddr.sin_zero, 0, 8);
	socklen_t serverlen = sizeof(servAddr);

	srand(time(NULL));
	uint16_t sequenceNumber = rand() % MAX_SEQUENCE_NUMBER;

	Packet syn(0, 1, 0, sequenceNumber, 0, 0, nullptr, 0);

	if (sendto(fd, (void *) &syn, sizeof(syn), 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
		cerr << "Error sending syn" << endl;
		exit(23);
	}

	Packet packet;
	Header header;
	uint16_t flags;

	sequenceNumber++;
	if (sequenceNumber >= MAX_SEQUENCE_NUMBER)
		sequenceNumber = 0;

	//std::cout << "Receiving SYN-ACK from server.\n";

	for (;;) {
		status = recvfrom(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&servAddr, &serverlen);
		if (status < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				int timeoutStatus = sendto(fd, (void *) &syn, sizeof(syn), 0, (struct sockaddr*)&servAddr, sizeof(servAddr));
				if (timeoutStatus < 0) {
					cerr << "Error re-sending SYN" << endl;
					exit(124);
				}
			}

			else {
				cerr << "Error receiving SYN-ACK from server" << endl;
				exit(84);
			}
		}

		else if (status == 0) {
			cerr << "Did not receive any data" << endl;
			break;
		}

		else {
			if (packet.getHeader().getFlags() == 6) {
				break;
			}

			else
				cerr << "Received improper packet (Not SYN-ACK)";
		}
	}

	header = packet.getHeader();

	Packet ack(1, 0, 0, header.getAckNum(), header.getSeqNum() + 1, 0, nullptr, 0);

	if (sendto(fd, (void *) &ack, sizeof(ack), 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
		cerr << "Error sending ack" << endl;
		exit(25);
	}

	sequenceNumber = (packet.getHeader().getSeqNum() + 1) % MAX_SEQUENCE_NUMBER;

	uint16_t ackNumber;

	// handshake completed, receiving file

	std::ofstream output("output.html", std::ofstream::app);

	int receivedLength = 0;
	int counter = 1;
	int timeout_flag = 0;
	for (;;) {
		// char* receivedPacket = new char[PACKET_LENGTH];
		Packet packet;
		receivedLength = recvfrom(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&servAddr, &serverlen);
		//cout << "Received length is: " << receivedLength << endl;
		if(packet.getFlag() == 1)
		{
			cerr << "negrooo0" << endl;
			timeout_flag = 1;
		}
		else 
		{
			cerr << "poop" << endl;
		}
		if (receivedLength < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				cerr << "kill yourself baka" << endl;
				timeout_flag = 1;
				continue;
			}

			else {
				cerr << "Error in recvfrom" << endl;
				exit(100);
			}
		}

		else if (receivedLength == 0) {
			cerr << "Did not receive any data" << endl;
			exit(125);
		}

		ackNumber = (packet.getHeader().getSeqNum() + packet.getHeader().getWindowSize()) % MAX_SEQUENCE_NUMBER;

		if (receivedLength > 0) {
			// got a FIN
			if (packet.getHeader().getFlags() == 1) {
				
				ackNumber += 1;
				ackNumber %= MAX_SEQUENCE_NUMBER;

				// send FIN-ACK
				Packet finAck(0, 1, 1, sequenceNumber, ackNumber, 0, nullptr, 0);

				if (sendto(fd, (void *) &finAck, sizeof(finAck), 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
					cerr << "Error sending fin-ack" << endl;
					cerr << "baka yaro" << endl;
					exit(23);
				}

				// Client's FIN and FIN-ACK down here, will fix later

				// Packet fin(false, false, true, sequenceNumber, ackNumber+1, 0, "");
				// memcpy(buf, &fin, 8);


				// if (sendto(fd, buf, 8, 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
				// 	cerr << "Error sending fin" << endl;
				// 	exit(23);
				// }

				// receivedLength = recvfrom(fd, buf, 8, 0, (struct sockaddr*)&servAddr, &serverlen);
				// if (receivedLength < 0) {
				// 	cout << "Error in recvfrom" << endl;
				// 	exit(100);
				// }

				break;
			}

			else if (packet.getHeader().getFlags() == 0){
				cout << "Receiving data packet " << packet.getHeader().getSeqNum() << endl;
				output.write(packet.getData(), packet.getHeader().getWindowSize());				//NOT SURE HOW THIS WILL WORK WITH THE IMPLEMENTATION
				//cout << packet.getData() << endl;

				packet = Packet(1, 0, 0, sequenceNumber, ackNumber, 0, nullptr, 0);

				if (sendto(fd, (void *) &packet, sizeof(packet), 0, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
					cerr << "yuuuuuuuuuuuuu " << endl;
					cerr << "Error sending ACK for received packet.\n";
				}
				if(timeout_flag == 0)
				{
					cout << "Sending ACK packet " << ackNumber << endl;
				}
				else if(timeout_flag)
				{
					cout << "Sending ACK packet " << ackNumber << " Retransmission" << endl;
					timeout_flag = 0;
				}
				cout << "counter = " << counter << endl;
				counter++;

				sequenceNumber += 1;

				sequenceNumber %= MAX_SEQUENCE_NUMBER;
			}

			else {
				cerr << "Neither FIN nor data packet" << endl;
				continue;
			}
		}

		else {
			cerr << "Received no data, leaving infinite loop" << endl;
			break;
		}
	}
	cerr << "we did it boys" << endl;
	return 0;
}
