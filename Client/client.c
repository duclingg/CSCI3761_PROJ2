#include<stdio.h>
#include<stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/select.h>

#define MSS 17 // max segment size
#define WINDOWSIZE 10 // max number of data bytes in the send window

int main(int argc, char *argv[]) {
	int n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int seqNumber = 0;
	int sizeOfSendBuffer = 0;
	char bufferOut[MSS];
	char bufferIn[MSS];
	
	// check for correct number of command line arguments
	if (argc < 3) {
		fprintf(stderr, "Usage is %s <ip_address> <portno> \n", argv[0]);
		exit(0);
	}

	// get server ip address and port number
    server = gethostbyname(argv[1]);
	int portno = atoi(argv[2]);
	if (server == NULL) {
		fprintf(stderr, "Error: no such host\n");
		exit(0);
	}

	// create the socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0) {
		perror("Error opening socket");
		exit(1);
	}

	// set the server address
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);
	serv_addr.sin_port = htons(portno);

	// prompt user for string to send
    char string[1024];
	printf("Enter a string to send to the server: ");
	fgets(string, sizeof(string), stdin);

	sizeOfSendBuffer = strlen(string);
    printf("Total bytes sent is %d\n", sizeOfSendBuffer);

	// send size of message to server
	int size = htonl(sizeOfSendBuffer);
	n = sendto(sockfd, &size, sizeof(size), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	if (n < 0) {
		perror("Error: failed to send size of message");
		exit(1);
	}

	// set up the sliding window
	int windowStart = 1;
	int windowEnd = WINDOWSIZE;

    time_t MAXWAITITME = 5;

	// send the message to the server
	while (windowStart <= windowEnd) {
        // calculate number of segments needed
		for (int i = 0; i < size; i++) {
			// calculate how much data to send
        	int dataToSend = (i + 2 <= size) ? 2 : 1; // send 2 bytes at a time unless it's the last segment

			// fill buffer with data and delimiter
			bzero(bufferOut, MSS);
			sprintf(bufferOut, "%11d%4d", seqNumber, dataToSend);
			strncpy(bufferOut, string + i, dataToSend);

			n = sendto(sockfd, bufferOut, MSS, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
			if (n < 0) {
				perror("Error: failed to send segment");
				exit(1);
			}
		}

		// increment sequence number
		seqNumber++;

		struct timeval timeout;
		timeout.tv_sec = MAXWAITITME;
		timeout.tv_usec = 0;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(sockfd, &readfds);

		// wait for data to be available or timeout
		int selectResult = select(sockfd + 1, &readfds, NULL, NULL, &timeout);
		if(selectResult == -1) {
			perror("Error in select");
			exit(1);
		} else if (selectResult == 0) {
			// timeout occured, resend the data
			n = sendto(sockfd, bufferOut, MSS, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
			if (n < 0) {
				perror("Error: failed to resend message");
				exit(1);
			}
		} else {
			if (FD_ISSET(sockfd, &readfds)) {
				// data is available to read from the socket
				bzero(bufferIn, MSS);
				n = recvfrom(sockfd, bufferIn, MSS, 0, NULL, NULL);
				if(n < 0) {
					perror("Error: failed to receive ACK");
					exit(1);
				}

				// parse the ACK 
				int ackNum;
				sscanf(bufferIn, "%11d", &ackNum);

				// check if the recevied ACK is vailed
				if (ackNum >= windowStart) {
					windowStart = ackNum + 1;
					seqNumber = windowStart;
					break;
				}

				if (ackNum == seqNumber) {
					break;
				}
			}
		}
		
		// update window boundaries
		windowEnd = windowStart + WINDOWSIZE - 1;

		// check if window has reach the end of the data
		if (windowEnd > sizeOfSendBuffer) {
			windowEnd = sizeOfSendBuffer;
		}
	}

	// receive confirmation from server that the message was received
	bzero(bufferIn, MSS);
	n = recvfrom(sockfd, bufferIn, MSS, 0, NULL, NULL);
	if (n < 0) {
		perror("Error: failed to receive confirmation");
		exit(1);
	}

	printf("Message successfully sent to server!\n");
	
	close(sockfd);
	return 0;
}