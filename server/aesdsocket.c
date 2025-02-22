#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

#define PORT 9000
#define BUFFER_SIZE 1024
#define FILE_PATH "/var/tmp/aesdsocketdata"

int server_fd;

// Handle SIGINT and SIGTERM for graceful shutdown
void signal_handler(int signum) {
    syslog(LOG_INFO, "Caught signal, exiting");
    printf("\nCaught signal, exiting...\n");

    if (server_fd != -1) {
        close(server_fd);
    }

    remove(FILE_PATH);
    closelog();
    exit(EXIT_SUCCESS);
}

// Logs client's IP address
void log_client_ip(int client_socket, char *client_ip, int is_closed) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (getpeername(client_socket, (struct sockaddr *)&client_addr, &addr_len) == -1) {
        perror("getpeername failed");
        syslog(LOG_ERR, "getpeername failed");
        return;
    }

    if (inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN) == NULL) {
        perror("inet_ntop failed");
        syslog(LOG_ERR, "inet_ntop failed");
        return;
    }

    if (!is_closed) {
        syslog(LOG_INFO, "Accepted Connection from %s", client_ip);
        printf("Accepted Connection from %s\n", client_ip);
    } else {
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
        printf("Closed connection from %s\n", client_ip);
    }
}

// Handles client communication
void handle_client(int client_fd) {
    char buffer[BUFFER_SIZE], client_ip[INET_ADDRSTRLEN];
    ssize_t bytes_received, bytes_read;

    log_client_ip(client_fd, client_ip, 0);

    FILE *client_data = fopen(FILE_PATH, "a+");
    if (!client_data) {
        perror("File open failed");
        close(client_fd);
        return;
    }

    printf("Receiving data...\n");

    while ((bytes_received = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytes_received] = '\0';

        printf("%.*s", (int)bytes_received, buffer);
        if (fwrite(buffer, 1, bytes_received, client_data) < (size_t)bytes_received) {
            perror("File write failed");
            break;
        }

        fflush(client_data);

        if (strchr(buffer, '\n')) {
            FILE *send_data = fopen(FILE_PATH, "r");
            if (!send_data) {
                perror("File open failed for reading");
                break;
            }

            printf("\nSending file contents:\n");
            while ((bytes_read = fread(buffer, 1, sizeof(buffer), send_data)) > 0) {
                if (send(client_fd, buffer, bytes_read, 0) == -1) {
                    perror("Send failed");
                    fclose(send_data);
                    break;
                }
                printf("%.*s", (int)bytes_read, buffer);
            }
            fclose(send_data);
            printf("\n");
        }
    }

    log_client_ip(client_fd, client_ip, 1);
    fclose(client_data);
    close(client_fd);
}

// Runs the program as a daemon
void run_as_daemon() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }
    if (pid > 0) {
        exit(EXIT_SUCCESS);  // Parent exits
    }

    // Child process continues as a daemon
    if (setsid() < 0) {
        perror("setsid failed");
        exit(EXIT_FAILURE);
    }

    // Redirect standard files to /dev/null
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    
    open("/dev/null", O_RDWR);  // stdin
    dup(0);                     // stdout
    dup(0);                     // stderr
}

int main(int argc, char *argv[]) {
    struct sockaddr_in serv_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int client_fd;
    int daemon_mode = 0;

    // Parse command-line arguments
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = 1;
    }

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    openlog("MyServer", LOG_PID | LOG_CONS, LOG_USER);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        perror("Socket creation failed");
        syslog(LOG_ERR, "Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("Bind failed");
        syslog(LOG_ERR, "Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (daemon_mode) {
        run_as_daemon();
    }

    if (listen(server_fd, 5) == -1) {
        perror("Listen failed");
        syslog(LOG_ERR, "Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);
    syslog(LOG_INFO, "Server started, listening on port %d", PORT);

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd == -1) {
            perror("Accept failed");
            continue;
        }
        handle_client(client_fd);
    }

    return 0;
}
