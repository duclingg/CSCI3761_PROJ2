#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define WINDOW_SIZE 10

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <server_ip> <server_port>\n", argv[0]);
        exit(1);
    }

    const char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket == -1) {
        perror("Error creating socket");
        exit(1);
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(server_port);
    server_address.sin_addr.s_addr = inet_addr(server_ip);

    char input_string[1024];
    printf("Enter a string to send: ");
    fgets(input_string, sizeof(input_string), stdin);

    size_t string_length = strlen(input_string);
    printf("String length is: %zu\n", string_length);

    char* current_position = input_string;
    int seq_number = 1;

    while (string_length > 0) {
        int bytes_to_send = (string_length > WINDOW_SIZE) ? WINDOW_SIZE : string_length;
        char send_buffer[17];

        // format the sequence number and size into the send buffer
        sprintf(send_buffer, "%11d%4d", seq_number, bytes_to_send);

        // send size of message
        uint32_t size_encoded = htonl(strlen(send_buffer) + bytes_to_send);
        ssize_t sent_size_bytes = sendto(client_socket, &size_encoded, sizeof(uint32_t), 0, (struct sockaddr*)&server_address, sizeof(server_address));
        if (sent_size_bytes == -1) {
            perror("Error sending size to the server");
            close(client_socket);
            exit(1);
        }

        // send message
        ssize_t sent_message_bytes = sendto(client_socket, send_buffer, 17, 0, (struct sockaddr*)&server_address, sizeof(server_address));
        if (sent_message_bytes == -1) {
            perror("Error sending message to the server");
            close(client_socket);
            exit(1);
        }

        current_position += bytes_to_send;
        string_length -= bytes_to_send;
        seq_number++;
    }

    // recieve and send messages to server
    char received_buffer[1024];
    ssize_t received_bytes;
    int received_seq_number, received_size;

    while ((received_bytes = recvfrom(client_socket, received_buffer, sizeof(received_buffer), 0, NULL, NULL)) > 0) {
        if (sscanf(received_buffer, "%11d%4d", &received_seq_number, &received_size) == 2) {
            received_buffer[received_size] = '\0';
            printf("Received Seq %d, Size %d: %s\n", received_seq_number, received_size, received_buffer);
        }
    }   

    close(client_socket);

    return 0;
}