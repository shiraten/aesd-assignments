#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define PORT "9000"
#define BACKLOG 5
#define FILEPATH "/var/tmp/aesdsocketdata"
#define BUF_SIZE 1024
#define TIME_BUF_SIZE 50

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct thread_data {
    pthread_t thread_id;
    int client_fd;
    struct sockaddr *client_addr;
    SLIST_ENTRY(thread_data) entries;
} thread_data_t;

SLIST_HEAD(thread_list, thread_data) thread_head;

volatile sig_atomic_t stop = 0;
volatile sig_atomic_t write_timestamp = 0;


void handle_interrupt(int signal) {
    stop = 1;
    fprintf(stdout, "Signal received, exiting\n");
    syslog(LOG_INFO, "Signal received, exiting\n");
}

void handle_alarm(int s) {
     write_timestamp = 1;
}

void daemonize()
{
    pid_t pid;

    /* Fork off the parent process */
    pid = fork();

    /* An error occurred */
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* Success: Let the parent terminate */
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    /* On success: The child process becomes session leader */
    if (setsid() == -1) {
        exit(EXIT_FAILURE);
    }
}

void* timestamp_handler() {
    int file_fd, rc;
    char buffer[TIME_BUF_SIZE];
    time_t t;
    struct tm *tmp;
    size_t time_len;

    alarm(10);
    while(!stop) {
        if(write_timestamp) {
            write_timestamp = 0;
            /* get timestamp to buffer */
            memset(buffer, '\0', sizeof(buffer));
            time(&t);
            tmp = localtime(&t);
            time_len = strftime(buffer, sizeof(buffer), "%F-%H:%M:%S\n", tmp);  /* %F is equivalent to %Y-%m-%d */
            /* lock mutex */
            printf("alrm lock\n");
            rc = pthread_mutex_lock(&mutex);
            if(rc != 0) {
                syslog(LOG_ERR, "pthread_mutex_lock failed wiht %d\n", rc);
                fprintf(stderr, "pthread_mutex_lock failed wiht %d\n", rc);
            }
            /* open file */
            file_fd = open(FILEPATH, O_CREAT | O_APPEND | O_RDWR, 0644);
            if(file_fd == -1) {
                fprintf(stderr, "cannot open file %s: %s\n", FILEPATH, strerror(errno));
                syslog(LOG_ERR, "cannot open file %s: %s\n", FILEPATH, strerror(errno));
                pthread_mutex_unlock(&mutex);
                continue;
            }
            /* write to file */
            if (write(file_fd, buffer, time_len) == -1) {
                syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
                fprintf(stderr, "Failed to write to file: %s", strerror(errno));
            }
            close(file_fd);
            /* unlock mutex */
            printf("alrm unlock\n");
            rc = pthread_mutex_unlock(&mutex);
            if(rc != 0) {
                syslog(LOG_ERR, "pthread_mutex_unlock failed wiht %d\n", rc);
                fprintf(stderr, "pthread_mutex_unlock failed wiht %d\n", rc);
            }
            alarm(10);
        }
    }
    pthread_exit(NULL);
}


void* client_handler(void *thread_param) {
    char client_ip[INET6_ADDRSTRLEN];
    char buffer[BUF_SIZE];
    char buffer_file[BUF_SIZE];
    ssize_t bytes_read;
    ssize_t bytes_rcv;
    int file_fd;
    int rc;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct sockaddr *client_addr = thread_func_args->client_addr;

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
        pthread_exit(NULL);
    }
    /* receive data from client */
    while ((bytes_rcv = recv(thread_func_args->client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        /* Null-terminate the received data to treat it as a string */
        buffer[bytes_rcv] = '\0';
        /* lock mutex */
        printf("client lock\n");
        rc = pthread_mutex_lock(&mutex);
        if(rc != 0) {
            syslog(LOG_ERR, "pthread_mutex_lock failed wiht %d\n", rc);
            fprintf(stderr, "pthread_mutex_lock failed wiht %d\n", rc);
        }
        /* Write received data from client to file */
        if (write(file_fd, buffer, bytes_rcv) == -1) {
            syslog(LOG_ERR, "Failed to write to file: %s", strerror(errno));
            close(file_fd);
            close(thread_func_args->client_fd);
            pthread_mutex_unlock(&mutex);
            pthread_exit(NULL);
        }
        /* unlock mutex */
        printf("client unlock\n");
        rc = pthread_mutex_unlock(&mutex);
        if(rc != 0) {
            syslog(LOG_ERR, "pthread_mutex_unlock failed wiht %d\n", rc);
            fprintf(stderr, "pthread_mutex_unlock failed wiht %d\n", rc);
        }
        /* Check for newline. As soon as newline is found, data is considered complete,
         * then send the file content back to the client
         */
        if (strchr(buffer, '\n')) {
            lseek(file_fd, 0, SEEK_SET);
            while ((bytes_read = read(file_fd, buffer_file, sizeof(buffer_file))) > 0) {
                if (send(thread_func_args->client_fd, buffer_file, bytes_read, 0) == -1) {
                    syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
                    close(file_fd);
                    close(thread_func_args->client_fd);
                    pthread_exit(NULL);
                }
            }
        }
    }
    if (bytes_rcv < 0) {
        syslog(LOG_ERR, "rcv failed: %s\n", strerror(errno));
    }
    syslog(LOG_INFO, "Closed connection from %s\n", client_ip);

    close(file_fd);
    close(thread_func_args->client_fd);
    pthread_exit(NULL);
}

void cleanup_threads() {
    thread_data_t *thread_data;
    while (!SLIST_EMPTY(&thread_head)) {
        thread_data = SLIST_FIRST(&thread_head);
        SLIST_REMOVE_HEAD(&thread_head, entries);
        pthread_join(thread_data->thread_id, NULL);
        free(thread_data);
    }
}


int main(int argc, char *argv[]) {
    struct thread_data* thread_param;

    struct sigaction sa, st;

    int rc;
    int server_fd, client_fd;
    struct sockaddr_storage client_addr;
    int daemon = 0;
    struct addrinfo hints, *servinfo;
    thread_param = malloc(sizeof(struct thread_data));
    if(thread_param == NULL) {
        syslog(LOG_ERR, "failed to allocate memory");
        return -1;
    }
    socklen_t client_addr_len = sizeof(client_addr);

    if(argc == 2) {
        if(!strcmp(argv[1], "-d")) {
            daemon = 1;
        }
    }

    /* interupt signal */
    sa.sa_handler = handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set up signal handlers: %s\n", strerror(errno));
        free(thread_param);
        return -1;
    }

    /* alarm signal */
    st.sa_handler = handle_alarm;
    sigemptyset(&st.sa_mask);
    st.sa_flags = 0;

    if (sigaction(SIGALRM, &st, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set up signal handlers: %s\n", strerror(errno));
        free(thread_param);
        return -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s\n", strerror(errno));
        free(thread_param);
        return -1;
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        syslog(LOG_ERR, "Failed to set socket options: %s\n", strerror(errno));
        close(server_fd);
        free(thread_param);
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
        free(thread_param);
        return -1;
    }

    if (bind(server_fd, servinfo->ai_addr, servinfo->ai_addrlen) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close(server_fd);
        free(thread_param);
        return -1;
    }
    freeaddrinfo(servinfo);

    if (listen(server_fd, BACKLOG) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s\n", strerror(errno));
        close(server_fd);
        free(thread_param);
        return -1;
    }

    syslog(LOG_INFO, "Server listening on port %s\n", PORT);

    if(daemon){
        daemonize();
    }

    // pthread_mutex_init (&mutex, NULL);

    /* timestamp thread */
    pthread_t timer_tid;
    rc = pthread_create(&timer_tid, NULL, timestamp_handler, NULL);
    if(rc != 0) {
        syslog(LOG_ERR, "pthread_create failed wiht %d\n", rc);
        return -1;
    }

    while (!stop) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        if(client_fd == -1) {
            if (errno == EINTR) break;
            syslog(LOG_ERR, "Failed to accept connection: %s\n", strerror(errno));
            break;
        }
        thread_param->client_addr = (struct sockaddr *)&client_addr;
        thread_param->client_fd = client_fd;
        rc = pthread_create(&thread_param->thread_id, NULL, client_handler, thread_param);
        if(rc != 0) {
            syslog(LOG_ERR, "pthread_create failed wiht %d\n", rc);
            close(client_fd);
            free(thread_param);
            return -1;
        }
        SLIST_INSERT_HEAD(&thread_head, thread_param, entries);
    }

    pthread_join(timer_tid, NULL);
    syslog(LOG_INFO, "Timer thread stopped");

    cleanup_threads();

    if (close(server_fd) == -1) {
        syslog(LOG_ERR, "Failed to close server socket: %s\n", strerror(errno));
    }
    if (remove(FILEPATH) != 0) {
        syslog(LOG_ERR, "Failed to delete file: %s\n", strerror(errno));
    }
    return 0;
}
