#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>

#define PORT "9000"
#define BACKLOG 5

int main()
{
    FILE *fp;
    char *filepath = "/var/tmp/aesdsocketdata";
    int sockfd = 0;
    int status = 0;
    int client_fd = 0;
    int bytes_rcv = 0;
    char buf[128];
    struct addrinfo hints;
    struct addrinfo *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t addr_size;

    int i = 0;
    char substring[128];
    int previous_occurence = 0;
    int new_occurence = 0;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if((status = getaddrinfo(NULL, PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
        goto close_socket;
    }

    sockfd = socket(servinfo->ai_family ,servinfo->ai_socktype, servinfo->ai_protocol);
    if(sockfd == -1) {
        fprintf(stderr, "*** ERROR: socket failed with return value %d: %s\n", errno, strerror(errno));
        syslog(LOG_ERR, "*** ERROR: socket failed with return value %d: %s\n", errno, strerror(errno));
        return 1;
    }
    if(bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen) != 0) {
        fprintf(stderr, "*** ERROR: bind failed with return value %d: %s\n", errno, strerror(errno));
        syslog(LOG_ERR, "*** ERROR: bind failed with return value %d: %s\n", errno, strerror(errno));
        freeaddrinfo(servinfo);
        goto close_socket;
    }
    freeaddrinfo(servinfo);
    if (listen(sockfd, BACKLOG) < 0) {
        fprintf(stderr, "*** ERROR: listen failed with return value %d: %s\n", errno, strerror(errno));
        syslog(LOG_ERR, "*** ERROR: listen failed with return value %d: %s\n", errno, strerror(errno));
        goto close_socket;
    }
    addr_size = sizeof client_addr;
    if((client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &addr_size)) == -1) {
        fprintf(stderr, "*** ERROR: accept failed with return value %d: %s\n", errno, strerror(errno));
        syslog(LOG_ERR, "*** ERROR: accept failed with return value %d: %s\n", errno, strerror(errno));
        goto close_socket;
    }
    syslog(LOG_DEBUG, "Accepted connection from %s:%s\n", servinfo->ai_addr->sa_data, PORT);

    fp = fopen(filepath, "a+");
    if(fp == NULL) {
        fprintf(stderr, "cannot open file %s: %s", filepath, strerror(errno));
        syslog(LOG_ERR, "cannot open file %s: %s", filepath, strerror(errno));
        goto close_accept;
    }
    while((bytes_rcv = recv(client_fd, buf, (sizeof(buf) - 1), MSG_CMSG_CLOEXEC)) > 0) {
        /* add null terminated character a the end of buffer */
        buf[bytes_rcv] = '\0';
        /* split buffer on newline character */
        do {
            if(buf[i] == '\n' || buf[i] == '\0') {
                new_occurence = i;
                memcpy(substring, &buf[previous_occurence], (new_occurence - previous_occurence));
                previous_occurence = new_occurence;
                /* write buffer to file */
                if(fprintf(fp, "%s", substring) < 0) {
                    fprintf(stderr, "cannot write to file %s: %s", filepath, strerror(errno));
                    syslog(LOG_ERR, "cannot write to file %s: %s", filepath, strerror(errno));
                    goto clean_all;
                }
            }
            i++;
        } while(buf[i] != '\0');
    }
    if(bytes_rcv == -1) {
        fprintf(stderr, "*** ERROR: recv failed with return value %d: %s\n", errno, strerror(errno));
        syslog(LOG_ERR, "*** ERROR: recv failed with return value %d: %s\n", errno, strerror(errno));
        goto close_accept;
    }
    /* read back file and send data to client */
    // TODO

    fclose(fp);
    close(client_fd);
    close(sockfd);

    return 0;


clean_all:
    fclose(fp);
close_accept:
    close(client_fd);
close_socket:
    close(sockfd);

    return 1;
}