#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <ctype.h>

int sendStuff(int sockfd, struct sockaddr_in serv_addr, char *buffer);
char *rtrim(char *s);

#define WINDOWSIZE 10;

int main(int argc, char *argv[]) {
	int n = 0;
	int sockfd;
	int stringSize;
	struct sockaddr_in serv_addr;
	char buffer[100];
	int portNo;
	char serverip[29];

	if (argc < 3) {
		printf("usage is client <ipaddr> <portnumber>\n");
		exit(1);
	}

	printf("Enter the string you'd like to send: ");
	memset(buffer, '\0', 100);
	char *ptr = fgets(buffer, sizeof(buffer), stdin);

	if (ptr == NULL) {
		perror("Error: please enter a string");
		exit(1);
	}

	int sendSize = strlen(buffer);
	//printf("%s\n", buffer);
	//printf("%d\n",sendSize);
	if (sendSize > 99) {
		printf("Error: can't send more than 99 characters");
		exit(1);
	}

	buffer[sendSize - 1] = 0;

	// create the socket
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(serverip, argv[1]);
	portNo = strtol(argv[2], NULL, 10);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portNo);
	serv_addr.sin_addr.s_addr = inet_addr(serverip); 

	stringSize = htonl(sendSize);
	n = sendto(sockfd, &stringSize, sizeof(stringSize), 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
	sendStuff(sockfd, serv_addr, buffer);
	printf("String length: %d\n", sendSize);

	return 0;
}

int sendStuff(int sockfd, struct sockaddr_in serv_addr, char *buffer) {
	int n;
	char bufferOut[18];
	char bufferRead[1000];
	int seqNum = 0;
	time_t sendTime, currentTime;
	int stringSize = strlen(buffer);
	int ackNum;

	struct sockaddr_in fromAddr;
	socklen_t fromLen = sizeof(struct sockaddr_in);

	for (int i = 0; i < stringSize; i = i + 2) {
    	if (i == stringSize - 1) {
      		sprintf(bufferOut, "%11d%4d%c", seqNum, 2, buffer[i]);
		} else {
      		sprintf(bufferOut, "%11d%4d%c%c", seqNum, 2, buffer[i], buffer[i + 1]);
		}
	
		// fix length (stringSize)
		printf("str is '%s', string length is %d\n", buffer, stringSize);
		printf("sending packet %d, '%s', packet length is %zu\n", seqNum, bufferOut, strlen(bufferOut));
		sendto(sockfd, bufferOut, 17, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		sendTime = time(NULL);
		
		for (;;) {
			memset(bufferRead, 0, 100);
			n = recvfrom(sockfd, &bufferRead, 100, MSG_DONTWAIT, (struct sockaddr *)&fromAddr, &fromLen);
			
			if (n > 0) {
				sscanf(bufferRead, "%11d", &ackNum);
				printf("Received ACK %d\n", ackNum);
				seqNum += 2;
				break;
			}
			currentTime = time(NULL);
			
			// fix timeout
			if ((currentTime - sendTime) > 2) {
				if (i == stringSize - 1) {
					sprintf(bufferOut, "%11d%4d%c", seqNum, 2, buffer[i]);
				} else {
					sprintf(bufferOut, "%11d%4d%c%c", seqNum, 2, buffer[i], buffer[i + 1]);
					sendto(sockfd, bufferOut, 17, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
					sendTime = time(NULL);
				}
			}
		}
	}
  	return 0;
}