#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>

#define PORT "9000"
#define BACKLOG 5
#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUF_SIZE 1024

volatile sig_atomic_t stop = 0;


void signal_handler(int signal) {
    stop = 1;
    fprintf(stdout, "Signal received, exiting\n");
    syslog(LOG_INFO, "Signal received, exiting\n");
}


void client_handler(int client_fd, struct sockaddr *client_addr) {
    char client_ip[INET6_ADDRSTRLEN];
    char buffer[BUF_SIZE];
    char buffer_file[BUF_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_rcv;
    int file_fd;

    /* init buffer */
    memset(buffer, '\0', sizeof(buffer)); 
    memset(buffer_file, '\0', sizeof(buffer_file)); 
    memset(client_ip, 0, sizeof(client_ip));    
    if (client_addr->sa_family == AF_INET) {
        inet_ntop(AF_INET, &((struct sockaddr_in *)client_addr)->sin_addr, client_ip, INET_ADDRSTRLEN);
    } else if (client_addr->sa_family == AF_INET6) {
        inet_ntop(AF_INET6, &((struct sockaddr_in6 *)client_addr)->sin6_addr, client_ip, INET6_ADDRSTRLEN);
    }
    fprintf(stdout, "Accepted connection from %s\n", client_ip);
    syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

    /* open file to append data received from client*/
    file_fd = open(FILEPATH, O_CREAT | O_APPEND | O_RDWR, 0644);
    if(file_fd == -1) {
        fprintf(stderr, "cannot open file %s: %s\n", FILEPATH, strerror(errno));
        syslog(LOG_ERR, "cannot open file %s: %s\n", FILEPATH, strerror(errno));
        close(file_fd);
    }
    while ((bytes_rcv = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        /* Null-terminate the received data to treat it as a string */
        buffer[bytes_rcv] = '\0';
        /* Write received data from client to file */
        if (write(file_fd, buffer, bytes_rcv) == -1) {
            syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
            close(file_fd);
            close(client_fd);
        }
        /* Check for newline. As soon as newline is found, data is considered complete,
         * then send the file content back to the client
         */
        if (strchr(buffer, '\n')) {
            lseek(file_fd, 0, SEEK_SET);
            while ((bytes_read = read(file_fd, buffer_file, sizeof(buffer_file))) > 0) {
                if (send(client_fd, buffer_file, bytes_read, 0) == -1) {
                    syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
                    close(file_fd);
                    close(client_fd);
                }
            }
        }
    }
    if (bytes_rcv < 0) {
        syslog(LOG_ERR, "rcv failed: %s\n", strerror(errno));
    }
    syslog(LOG_INFO, "Closed connection from %s\n", client_ip);
    
    close(file_fd);
    close(client_fd);
}


int main(int argc, char *argv[]) {

    struct sigaction sa;

    int server_fd, client_fd;
    int daemon = 0;
    struct addrinfo hints, *servinfo;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    if(argc == 2) {
        if(!strcmp(argv[1], "-d")) {
            daemon = 1;
        }
    }

    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set up signal handlers: %s\n", strerror(errno));
        return -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s\n", strerror(errno));
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "Failed to set socket options: %s\n", strerror(errno));
        close(server_fd);
        return -1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int status = getaddrinfo(NULL, PORT, &hints, &servinfo);
    if (status != 0) {
        syslog(LOG_ERR, "getaddrinfo: %s\n", gai_strerror(status));
        close(server_fd);
        return -1;
    }

    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close(server_fd);
        return -1;
    }
    freeaddrinfo(servinfo);

    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s\n", strerror(errno));
        close(server_fd);
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %s\n", PORT);

    while (!stop) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if (client_fd == -1) {
            if (errno == EINTR) break;
            syslog(LOG_ERR, "Failed to accept connection: %s\n", strerror(errno));
            break;
        }
        if(daemon) {
            printf("start in daemon: not implemented\n");
        } else {
            client_handler(client_fd, (struct sockaddr *)&client_addr);
        }
    }

    if (close(server_fd) == -1) {
        syslog(LOG_ERR, "Failed to close server socket: %s\n", strerror(errno));
    }
    if (remove(FILEPATH) != 0) {
        syslog(LOG_ERR, "Failed to delete file: %s\n", strerror(errno));
    }
    return 0;
}
