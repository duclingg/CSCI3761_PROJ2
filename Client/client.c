#include<stdio.h>
#include<stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define MSS 17 // max segment size
#define WINDOWSIZE 10 // max number of data bytes in the send window

int main(int argc, char *argv[]){
	int n;
	struct sockaddr_in serv_addr;
	struct hostent *server;
	int seqNumber = 0;
	int sizeOfSendBuffer = 0;
	char bufferOut[MSS];
	char bufferIn[MSS];
	
	// check for correct number of command line arguments
	if (argc < 3){
		fprintf(stderr, "Usage is %s <ip_address> <portno> \n", argv[0]);
		exit(0);
	}

	// get server ip address and port number
	int portno = atoi(argv[2]);
	server = gethostbyname(argv[1]);
	if (server == NULL){
		fprintf(stderr, "Error: no such host\n");
		exit(0);
	}

	// create the socket
	int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sockfd < 0){
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
	int ackNum = 0;

    time_t timeSent;
    time_t currentTime;
    time_t MAXWAITITME = 5;

	// send the message to the server
	while (windowStart <= windowEnd){
        // calculate how much data to send
        int dataToSend = (MSS - 15 < sizeOfSendBuffer - windowStart + 1) ? (MSS - 15) : (sizeOfSendBuffer - windowStart + 1);

		// fill the buffer with sequence number, size, and data
		bzero(bufferOut, MSS);
		sprintf(bufferOut, "%11d%4d", (seqNumber), dataToSend);
		strncat(bufferOut, string + windowStart - 1, dataToSend); //only send 2 bytes of data
		
        // send buffer to server
		n = sendto(sockfd, bufferOut, MSS, 0, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
		if (n < 0) {
			perror("Error: failed to send message");
			exit(1);
		}

        // update the timeSent
        timeSent = time(NULL);

		// increment sequence number
		seqNumber++;

		// receive ACK from server
		while (1) {
            bzero(bufferIn, MSS);
		    n = recvfrom(sockfd, bufferIn, MSS, 0, NULL, NULL);

            // check if data was received
		    if (n < 0) {
			    perror("Error: failed to receive ACK");
			    exit(1);
		    }

            // check for timeout
            currentTime = time(NULL);
            if (currentTime - timeSent > MAXWAITITME) {
                // resend data and reset timeSent
                n = sendto(sockfd, bufferOut, MSS, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
                if (n < 0) {
                    perror("Error: failed to resend message");
                    exit(1);
                }
                // update time for resend 
                timeSent = time(NULL);
            }
        }

		// parse the ACK
		sscanf(bufferIn, "%11d", &ackNum);

		// check if ACK is valid
		if (ackNum != windowStart){
			// resend data from the seq number received in the ACK
			windowStart = ackNum;
            windowEnd = ackNum + WINDOWSIZE;
		}

        // update the window boundaries
        windowStart += dataToSend;
        windowEnd = windowStart + WINDOWSIZE - 1;

		// check if window has reached the end of the data
		if (windowEnd > sizeOfSendBuffer){
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