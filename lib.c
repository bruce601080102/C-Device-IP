#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>

#include "lib.h"

// RFC 5389 Section 6 STUN Message Structure
struct STUNMessageHeader {
    // Message Type (Binding Request / Response)
    unsigned short type;

    // Payload length of this message
    unsigned short length;

    // Magic Cookie
    unsigned int cookie;

    // Unique Transaction ID
    unsigned int identifier[3];
};

#define XOR_MAPPED_ADDRESS_TYPE 0x0020

// RFC 5389 Section 15 STUN Attributes
struct STUNAttributeHeader {
    // Attibute Type
    unsigned short type;

    // Payload length of this attribute
    unsigned short length;
};

#define IPv4_ADDRESS_FAMILY 0x01;
#define IPv6_ADDRESS_FAMILY 0x02;

// RFC 5389 Section 15.2 XOR-MAPPED-ADDRESS
struct STUNXORMappedIPv4Address {
    unsigned char reserved;
    unsigned char family;
    unsigned short port;
    unsigned int address;
};

struct STUNServer{
    char* address;
    int port;
};

/* Get the external IPv4 address

@param server A STUN server
@param address A non-null buffer to store the public IPv4 address
@return 0 on success.
@warning This function returns
         -1 if failed to bind the socket;
         -2 if failed to resolve the given STUN server;
         -3 if failed to send the STUN request;
         -4 if failed to read from the socket (and timed out; default = 5s);
         -5 if failed to get the external address. */
int IPv4address(struct STUNServer server, char *address) {
    // Create a UDP socket
    int socketd = socket(AF_INET, SOCK_DGRAM, 0);

    // Local Address
    struct sockaddr_in *localAddress = malloc(sizeof(struct sockaddr_in));

    bzero(localAddress, sizeof(struct sockaddr_in));
    localAddress->sin_family = AF_INET;
    localAddress->sin_addr.s_addr = INADDR_ANY;
    localAddress->sin_port = htons(0);

    if (bind(socketd, (struct sockaddr *)localAddress, sizeof(struct sockaddr_in)) < 0) {
        free(localAddress);

        close(socketd);
        return -1;
    }

    // Remote Address, First resolve the STUN server address
    struct addrinfo *results = NULL;
    struct addrinfo *hints = malloc(sizeof(struct addrinfo));

    bzero(hints, sizeof(struct addrinfo));
    hints->ai_family = AF_INET;
    hints->ai_socktype = SOCK_STREAM;

    if (getaddrinfo(server.address, NULL, hints, &results) != 0) {
        free(localAddress);
        free(hints);

        close(socketd);
        return -2;
    }

    struct in_addr stunaddr;

    // `results` is a linked list, Read the first node
    if (results != NULL) {
        stunaddr = ((struct sockaddr_in *)results->ai_addr)->sin_addr;
    } else {
        free(localAddress);
        free(hints);
        freeaddrinfo(results);

        close(socketd);
        return -2;
    }

    // Create the remote address
    struct sockaddr_in *remoteAddress = malloc(sizeof(struct sockaddr_in));
    bzero(remoteAddress, sizeof(struct sockaddr_in));
    remoteAddress->sin_family = AF_INET;
    remoteAddress->sin_addr = stunaddr;
    remoteAddress->sin_port = htons(server.port);

    // Construct a STUN request
    struct STUNMessageHeader *request = malloc(sizeof(struct STUNMessageHeader));
    request->type = htons(0x0001);
    request->length = htons(0x0000);
    request->cookie = htonl(0x2112A442);

    for (int index = 0; index < 3; index++) {
        srand((unsigned int)time(0));
        request->identifier[index] = rand();
    }

    // Send the request
    if (sendto(socketd, request, sizeof(struct STUNMessageHeader), 0, (struct sockaddr *)remoteAddress, sizeof(struct sockaddr_in)) == -1) {
        free(localAddress);
        free(hints);
        freeaddrinfo(results);
        free(remoteAddress);
        free(request);

        close(socketd);
        return -3;
    }

    // Set the timeout
    struct timeval tv = {5, 0};
    setsockopt(socketd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(struct timeval));

    // Read the response
    char *buffer = malloc(sizeof(char) * 512);
    bzero(buffer, 512);
    long length = read(socketd, buffer, 512);

    if (length < 0) {
        free(localAddress);
        free(hints);
        freeaddrinfo(results);
        free(remoteAddress);
        free(request);
        free(buffer);

        close(socketd);
        return -4;
    }

    char *pointer = buffer;

    struct STUNMessageHeader *response = (struct STUNMessageHeader *)buffer;
    if (response->type == htons(0x0101)) {
        // Check the identifer
        for (int index = 0; index < 3; index++) {
            if (request->identifier[index] != response->identifier[index]){
                return -1;
            }
        }

        pointer += sizeof(struct STUNMessageHeader);
        while (pointer < buffer + length) {
            struct STUNAttributeHeader *header = (struct STUNAttributeHeader *)pointer;

            if (header->type == htons(XOR_MAPPED_ADDRESS_TYPE)) {
                pointer += sizeof(struct STUNAttributeHeader);

                struct STUNXORMappedIPv4Address *xorAddress = (struct STUNXORMappedIPv4Address *)pointer;
                unsigned int numAddress = htonl(xorAddress->address) ^ 0x2112A442;

                // Parse the IP address
                snprintf(address, 20, "%d.%d.%d.%d",
                         (numAddress >> 24) & 0xFF,
                         (numAddress >> 16) & 0xFF,
                         (numAddress >> 8) & 0xFF,
                         numAddress & 0xFF);

                free(localAddress);
                free(hints);
                freeaddrinfo(results);
                free(remoteAddress);
                free(request);
                free(buffer);

                close(socketd);
                return 0;
            }

            pointer += (sizeof(struct STUNAttributeHeader) + ntohs(header->length));
        }
    }

    free(localAddress);
    free(hints);
    freeaddrinfo(results);
    free(remoteAddress);
    free(request);
    free(buffer);

    close(socketd);
    return -5;
}

char *SourceIP(char *host, char *port) {   
    struct STUNServer servers[2] = {
        {host, atoi(port)}, {"stun.l.google.com" , 19302}
    };

    size_t len = sizeof(servers)/sizeof(servers[0]);
    for (int i = 0; i < len; i++) {
        // printf("%s, %d\n", servers[i].address, servers[i].port);
        char *address = malloc(sizeof(char) * 100);
        bzero(address, 100);

        int err = IPv4address(servers[i], address);
        if (err == 0 && strlen(address) > 0) {
            // printf("Public IP: %s\n", address);
            return address;
        }
    }
    return "nan";
}

char *IntranetIP() {
    const char *google_dns_server = "8.8.8.8";
    int dns_port = 53;

    struct sockaddr_in serv;
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    // Socket could not be created
    if (sock < 0) {
        return "nan";
    }

    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr(google_dns_server);
    serv.sin_port = htons(dns_port);

    int err = connect(sock, (const struct sockaddr *)&serv, sizeof(serv));
    if (err < 0) {
        return "nan";
    }

    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr *)&name, &namelen);
    if (err < 0) {
        return "nan";
    }

    char buffer[80];
    const char *p = inet_ntop(AF_INET, &name.sin_addr, buffer, sizeof(buffer));
    if (p != NULL) {
        char *b = buffer;
        return b;
    }
    return "nan";

    close(sock);
}