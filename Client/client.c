#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

#define WINDOWSIZE 10 // size of window
#define MSS 17	// max segment size
#define TIMEOUT 5 // timeout time in seconds

int sendStuff(int sockfd, struct sockaddr_in serv_addr, char *buffer);
char *rtrim(char *s);

int main(int argc, char *argv[]) {
	int n;
	int sockfd;
	int stringSize;
	struct sockaddr_in serv_addr;
	char buffer[100];
	int portNo;
	char serverip[29];

	// usage error
	if (argc < 3) {
		printf("usage is client <ipaddr> <portnumber>\n");
		exit(1);
	}

	// prompt user to input a string to send to the server
	printf("Enter the string you'd like to send: ");
	memset(buffer, '\0', 100);
	char *ptr = fgets(buffer, sizeof(buffer), stdin);

	// invalid input
	if (ptr == NULL) {
		perror("Error: please enter a string");
		exit(1);
	}

	// get size of the string
	int sendSize = strlen(buffer);
	if (sendSize > 99) {
		printf("Error: can't send more than 99 characters");
		exit(1);
	}

	buffer[sendSize - 1] = 0;

	// create the socket and connect to the server
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(serverip, argv[1]);
	portNo = strtol(argv[2], NULL, 10);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portNo);
	serv_addr.sin_addr.s_addr = inet_addr(serverip); 

	// send the string length to the server
	stringSize = htonl(sendSize-1);
	n = sendto(sockfd, &stringSize, sizeof(stringSize), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	sendStuff(sockfd, serv_addr, buffer);
	printf("String length: %d\n", sendSize-1);

	close(sockfd);
	return 0;
}

// method sends packets to the server
int sendStuff(int sockfd, struct sockaddr_in serv_addr, char *buffer) {
	int n;
	char bufferOut[MSS];
	char bufferRead[1000];
	int seqNum = 0;
	time_t sendTime, currentTime;
	int stringSize = strlen(buffer);
	int ackNum;
	int sendSize;

	struct sockaddr_in fromAddr;
	socklen_t fromLen = sizeof(struct sockaddr_in);

	int windowBottom = 0;
	int windowTop = WINDOWSIZE - 1;

	// while the bottom of the window is smaller than the string length
	while (windowBottom < stringSize) {
		for (int i = windowBottom; i <= windowTop; i += 2) {
			if (i >= stringSize)
				break;

			sendSize = 17;

			// send the number of bytes
			if(i == stringSize - 1) {
				// only 1 byte
				sprintf(bufferOut, "%11d%4d%c", seqNum, 1, buffer[i]);
				sendSize = 16;
			} else {
				// 2 bytes being sent
				sprintf(bufferOut, "%11d%4d%c%c", seqNum, 2, buffer[i], buffer[i+1]);
			}	

			// send the string in two byte incrememnts
			printf("str is '%s', strlen is %d\n", buffer, stringSize);
			printf("sending packet %d, '%s', packet length is %zu\n", seqNum, bufferOut, strlen(bufferOut));
			sendto(sockfd, bufferOut, sendSize, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
			sendTime = time(NULL);

			// continue looping until all ACKs are received
			for (;;) {
				// receive response from server
				memset(bufferRead, 0, 100);
				n = recvfrom(sockfd, &bufferRead, 100, MSG_DONTWAIT, (struct sockaddr *)&fromAddr, &fromLen);

				// check if response is received from server
				if (n > 0) {
					// parse the ACK
					sscanf(bufferRead, "%11d", &ackNum);
					printf("Received ACK %d\n", ackNum);

					seqNum+=2; // increment the sequence number to match with the ack

					// slide the window to the right
					while(windowBottom <= ackNum) {
						windowBottom+=2;
						if (windowTop < stringSize) {
							windowTop+=2;
						}
					}
					break;
				}	

				// check for timeout
				currentTime = time(NULL);
				if ((currentTime - sendTime) >= TIMEOUT) {
					printf("***** TIMEOUT should do a resend\n");
				
					// send the number of bytes
					if(i == stringSize - 1) {
						// only 1 byte
						sprintf(bufferOut, "%11d%4d%c", seqNum, 1, buffer[i]);
						sendSize = 16;
					} else {
						// 2 bytes being sent
						sprintf(bufferOut, "%11d%4d%c%c", seqNum, 2, buffer[i], buffer[i+1]);
					}
					
					// resend packet if timed out
					printf("resending packet %d, '%s', packet length is %zu\n", seqNum, bufferOut, strlen(bufferOut));
					sendto(sockfd, bufferOut, sendSize, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
					sendTime = time(NULL);
					
					// check if response is received from server
					if (n > 0) {
						// parse the ACK
						sscanf(bufferRead, "%11d", &ackNum);
						printf("Received ACK %d\n", ackNum);

						seqNum+=2; // increment the sequence number to match with the ack

						// slide the window to the right
						while(windowBottom <= ackNum) {
							windowBottom+=2;
							if (windowTop < stringSize) {
								windowTop+=2;
							}
						}
						break;
					}
				}
			}
		}
	}

	close(sockfd);
	return 0;
}